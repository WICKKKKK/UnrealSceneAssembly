"""TCP bridge that marshals MCP requests onto the Unreal editor thread."""

from __future__ import annotations

import json
import queue
import socket
import threading
import traceback
import uuid
from dataclasses import dataclass, field
from typing import Any

from .bridge_config import BridgeConfig


MAX_COMMANDS_PER_TICK = 32


class BridgeServerError(RuntimeError):
    """Raised when the Unreal-side bridge cannot start."""


@dataclass
class _PendingCommand:
    request_id: str
    command: str
    params: dict[str, Any]
    response: dict[str, Any] | None = None
    event: threading.Event = field(default_factory=threading.Event)


class MCPBridgeServer:
    def __init__(self, config: BridgeConfig):
        self.config = config
        self._queue: queue.Queue[_PendingCommand] = queue.Queue()
        self._stop_event = threading.Event()
        self._server_socket: socket.socket | None = None
        self._accept_thread: threading.Thread | None = None
        self._tick_handle: Any = None
        self._running = False

    @property
    def is_running(self) -> bool:
        return self._running

    def start(self) -> None:
        if self._running:
            return

        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.settimeout(0.5)

        try:
            server_socket.bind((self.config.bridge_host, self.config.bridge_port))
            server_socket.listen(16)
        except OSError as exc:
            server_socket.close()
            raise BridgeServerError(f"Unable to bind bridge socket on {self.config.bridge_address}: {exc}") from exc

        try:
            import unreal

            self._tick_handle = unreal.register_slate_post_tick_callback(self._tick)
        except Exception as exc:
            server_socket.close()
            raise BridgeServerError(f"Unable to register Unreal tick callback: {exc}") from exc

        self._server_socket = server_socket
        self._stop_event.clear()
        self._accept_thread = threading.Thread(target=self._accept_loop, name="SceneAssemblyMCPBridge", daemon=True)
        self._accept_thread.start()
        self._running = True

    def stop(self) -> None:
        if not self._running and self._server_socket is None:
            return

        self._running = False
        self._stop_event.set()

        if self._server_socket is not None:
            try:
                self._server_socket.close()
            except OSError:
                pass
            self._server_socket = None

        if self._tick_handle is not None:
            try:
                import unreal

                unreal.unregister_slate_post_tick_callback(self._tick_handle)
            except Exception:
                pass
            self._tick_handle = None

        if self._accept_thread is not None and self._accept_thread.is_alive():
            self._accept_thread.join(timeout=1.0)
        self._accept_thread = None

    def _accept_loop(self) -> None:
        while not self._stop_event.is_set():
            try:
                client_socket, address = self._server_socket.accept() if self._server_socket else (None, None)
            except socket.timeout:
                continue
            except OSError:
                break

            if client_socket is None:
                continue

            thread = threading.Thread(
                target=self._handle_client,
                args=(client_socket, address),
                name="SceneAssemblyMCPBridgeClient",
                daemon=True,
            )
            thread.start()

    def _handle_client(self, client_socket: socket.socket, address: Any) -> None:
        client_socket.settimeout(self.config.request_timeout_seconds + 1.0)
        try:
            with client_socket:
                with client_socket.makefile("rwb") as stream:
                    for raw_line in stream:
                        if self._stop_event.is_set():
                            break
                        response = self._handle_line(raw_line)
                        stream.write(_json_line(response))
                        stream.flush()
        except Exception as exc:
            if self.config.verbose:
                print(f"[SceneAssembly MCP Bridge] Client {address} disconnected with error: {exc}")

    def _handle_line(self, raw_line: bytes) -> dict[str, Any]:
        request_id = None
        try:
            request = json.loads(raw_line.decode("utf-8"))
            if not isinstance(request, dict):
                return _error_response(request_id, "Request must be a JSON object")

            request_id = str(request.get("id") or uuid.uuid4())
            command = request.get("command")
            params = request.get("params")
            if params is None:
                params = {}

            if not isinstance(command, str) or not command:
                return _error_response(request_id, "Request field 'command' must be a non-empty string")
            if not isinstance(params, dict):
                return _error_response(request_id, "Request field 'params' must be an object")

            pending = _PendingCommand(request_id=request_id, command=command, params=params)
            self._queue.put(pending)

            if not pending.event.wait(timeout=self.config.request_timeout_seconds):
                return _error_response(request_id, f"Command timed out after {self.config.request_timeout_seconds:.1f}s")

            return pending.response or _error_response(request_id, "Command finished without a response")
        except json.JSONDecodeError as exc:
            return _error_response(request_id, f"Invalid JSON request: {exc}")
        except Exception as exc:
            return _error_response(request_id, f"Bridge request failed: {exc}")

    def _tick(self, delta_seconds: float) -> None:
        for _ in range(MAX_COMMANDS_PER_TICK):
            try:
                pending = self._queue.get_nowait()
            except queue.Empty:
                break

            try:
                from .command_handlers import handle_command

                result = handle_command(pending.command, pending.params, self._context())
                pending.response = {
                    "id": pending.request_id,
                    "status": "success",
                    "result": result,
                }
            except Exception as exc:
                error: dict[str, Any] = {"message": str(exc)}
                if self.config.verbose:
                    error["traceback"] = traceback.format_exc()
                pending.response = {
                    "id": pending.request_id,
                    "status": "error",
                    "error": error,
                }
            finally:
                pending.event.set()

    def _context(self) -> dict[str, Any]:
        return {"config": self.config}


def _json_line(payload: dict[str, Any]) -> bytes:
    return (json.dumps(payload, ensure_ascii=True, separators=(",", ":")) + "\n").encode("utf-8")


def _error_response(request_id: str | None, message: str) -> dict[str, Any]:
    return {
        "id": request_id,
        "status": "error",
        "error": {"message": message},
    }
