"""Editor-facing controller for the UnrealSceneAssembly MCP runtime."""

from __future__ import annotations

import atexit
import base64
import json
import os
import threading
import traceback
from dataclasses import asdict
from typing import Any

from .bridge_config import BridgeConfig, load_config, with_mcp_port
from .bridge_server import MCPBridgeServer
from .process_manager import (
    MCPServerProcess,
    check_dependencies,
    check_tcp_port_available,
    find_python_executable,
    install_dependencies,
    install_hint,
)


def _json_result(ok: bool, **payload: Any) -> str:
    payload["ok"] = ok
    return json.dumps(payload, ensure_ascii=False, separators=(",", ":"))


def _safe_json(func):
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as exc:
            return _json_result(False, error=str(exc), traceback=traceback.format_exc())

    return wrapper


class SceneAssemblyMCPController:
    def __init__(self, plugin_python_dir: str | None = None):
        self.plugin_python_dir = plugin_python_dir
        self.config = load_config(plugin_python_dir)
        self.bridge: MCPBridgeServer | None = None
        self.process: MCPServerProcess | None = None
        self.install_thread: threading.Thread | None = None
        self.install_state: dict[str, Any] = {
            "running": False,
            "ok": None,
            "returncode": None,
            "message": "Not started.",
            "lines": [],
        }
        self._lock = threading.RLock()

    def configure_port(self, port: int) -> None:
        with self._lock:
            if self.is_running():
                raise RuntimeError("Stop MCP before changing the port.")
            self.config = with_mcp_port(self.config, int(port))
            self.process = None

    def is_running(self) -> bool:
        return bool(
            self.bridge is not None
            and self.bridge.is_running
            and self.process is not None
            and self.process.is_running
        )

    def start(self, port: int | None = None) -> dict[str, Any]:
        with self._lock:
            if self.is_running():
                return {"running": True, "message": "MCP is already running.", "config": self._config_summary()}

            if port is not None:
                self.configure_port(int(port))

            dependencies = check_dependencies(self.config)
            if not dependencies.installed:
                return {
                    "running": False,
                    "message": "MCP Python dependencies are not installed.",
                    "dependencies": asdict(dependencies),
                    "install_hint": install_hint(self.config),
                    "config": self._config_summary(),
                }

            port_status = check_tcp_port_available(self.config.mcp_host, self.config.mcp_port)
            if not port_status.available:
                return {
                    "running": False,
                    "message": "MCP HTTP port is not available.",
                    "port": asdict(port_status),
                    "config": self._config_summary(),
                }

            bridge = MCPBridgeServer(self.config)
            process = MCPServerProcess(self.config)
            try:
                bridge.start()
                if not process.start():
                    raise RuntimeError("External MCP server process did not start. See Output Log for details.")
            except Exception:
                process.stop()
                bridge.stop()
                raise

            self.bridge = bridge
            self.process = process
            return {"running": True, "message": "MCP started.", "config": self._config_summary()}

    def stop(self) -> dict[str, Any]:
        with self._lock:
            if self.process is not None:
                self.process.stop()
                self.process = None
            if self.bridge is not None:
                self.bridge.stop()
                self.bridge = None
            return {"running": False, "message": "MCP stopped.", "config": self._config_summary()}

    def start_install(self) -> dict[str, Any]:
        with self._lock:
            if self.install_thread is not None and self.install_thread.is_alive():
                return {"running": True, "message": "Dependency installation is already running.", "install": self._install_summary()}

            python_exe = find_python_executable()
            if not python_exe:
                return {"running": False, "message": "Unable to find a Python executable.", "install": self._install_summary()}

            self.install_state = {
                "running": True,
                "ok": None,
                "returncode": None,
                "message": "Installing dependencies...",
                "python": python_exe,
                "lines": [],
            }
            self.install_thread = threading.Thread(target=self._install_worker, args=(python_exe,), name="SceneAssemblyMCPInstall", daemon=True)
            self.install_thread.start()
            return {"running": True, "message": "Dependency installation started.", "install": self._install_summary()}

    def _install_worker(self, python_exe: str) -> None:
        def on_line(line: str) -> None:
            with self._lock:
                lines = self.install_state.setdefault("lines", [])
                lines.append(line)
                del lines[:-80]

        try:
            result = install_dependencies(self.config, python_exe=python_exe, output_callback=on_line)
            with self._lock:
                self.install_state.update(
                    {
                        "running": False,
                        "ok": result.ok,
                        "returncode": result.returncode,
                        "command": result.command,
                        "message": "Dependencies installed." if result.ok else "Dependency installation failed.",
                    }
                )
        except Exception as exc:
            with self._lock:
                self.install_state.update(
                    {
                        "running": False,
                        "ok": False,
                        "returncode": None,
                        "message": str(exc),
                        "traceback": traceback.format_exc(),
                    }
                )

    def _config_summary(self) -> dict[str, Any]:
        return {
            "mcp_host": self.config.mcp_host,
            "mcp_port": self.config.mcp_port,
            "mcp_path": self.config.mcp_path,
            "mcp_url": self.config.mcp_url,
            "bridge_host": self.config.bridge_host,
            "bridge_port": self.config.bridge_port,
            "bridge_address": self.config.bridge_address,
            "autostart_mcp": self.config.autostart_mcp,
            "server_dir": self.config.server_dir,
            "site_packages_dir": self.config.site_packages_dir,
            "server_script": self.config.server_script,
        }

    def _install_summary(self) -> dict[str, Any]:
        state = dict(self.install_state)
        state["lines"] = list(state.get("lines", []))[-20:]
        return state


_controller: SceneAssemblyMCPController | None = None


def get_controller(plugin_python_dir: str | None = None) -> SceneAssemblyMCPController:
    global _controller
    if _controller is None:
        _controller = SceneAssemblyMCPController(plugin_python_dir)
    elif plugin_python_dir is not None and os.path.abspath(plugin_python_dir) != os.path.abspath(_controller.config.plugin_python_dir):
        _controller = SceneAssemblyMCPController(plugin_python_dir)
    return _controller


def initialize(plugin_python_dir: str | None = None) -> SceneAssemblyMCPController:
    controller = get_controller(plugin_python_dir)
    if controller.config.autostart_mcp:
        controller.start()
    return controller


@_safe_json
def get_status_json() -> str:
    controller = get_controller()
    dependencies = check_dependencies(controller.config)
    port_status = check_tcp_port_available(controller.config.mcp_host, controller.config.mcp_port)
    return _json_result(
        True,
        running=controller.is_running(),
        config=controller._config_summary(),
        dependencies=asdict(dependencies),
        port=asdict(port_status),
        install=controller._install_summary(),
        python_executable=find_python_executable(),
        opencode_url_hint=controller.config.mcp_url,
    )


@_safe_json
def check_environment_json() -> str:
    controller = get_controller()
    dependencies = check_dependencies(controller.config)
    return _json_result(True, installed=dependencies.installed, dependencies=asdict(dependencies), install_hint=install_hint(controller.config), python_executable=find_python_executable())


@_safe_json
def check_port_json(port: int | str | None = None) -> str:
    controller = get_controller()
    check_port = int(port) if port is not None else controller.config.mcp_port
    port_status = check_tcp_port_available(controller.config.mcp_host, check_port)
    return _json_result(True, port=asdict(port_status))


@_safe_json
def install_dependencies_json() -> str:
    controller = get_controller()
    return _json_result(True, **controller.start_install())


@_safe_json
def start_json(port: int | str | None = None) -> str:
    controller = get_controller()
    start_port = int(port) if port not in (None, "") else None
    result = controller.start(start_port)
    return _json_result(bool(result.get("running")), **result)


@_safe_json
def stop_json() -> str:
    controller = get_controller()
    return _json_result(True, **controller.stop())


@_safe_json
def get_selected_whitebox_json() -> str:
    from . import assembly_test

    return _json_payload(assembly_test.get_selected_actor())


@_safe_json
def get_selection_summary_json() -> str:
    from . import assembly_test

    return _json_payload(assembly_test.get_selection_summary())


@_safe_json
def select_all_whiteboxes_json() -> str:
    from . import assembly_test

    return _json_payload(assembly_test.select_all_whiteboxes())


@_safe_json
def deselect_all_json() -> str:
    from . import assembly_test

    return _json_payload(assembly_test.deselect_all())


@_safe_json
def select_actor_by_path_json(payload_b64: str = "") -> str:
    from . import assembly_test

    return _json_payload(assembly_test.select_actor_by_path(_decode_payload(payload_b64)))


@_safe_json
def focus_actor_by_path_json(payload_b64: str = "") -> str:
    from . import assembly_test

    return _json_payload(assembly_test.focus_actor_by_path(_decode_payload(payload_b64)))


@_safe_json
def cleanup_assembly_results_json(payload_b64: str = "") -> str:
    from . import assembly_test

    return _json_payload(assembly_test.cleanup_assembly_results(_decode_payload(payload_b64)))


@_safe_json
def compute_dual_image_rotation_json(payload_b64: str = "") -> str:
    from . import assembly_test

    return _json_payload(assembly_test.compute_dual_image_rotation(_decode_payload(payload_b64)))


@_safe_json
def compute_precomputed_rotation_json(payload_b64: str = "") -> str:
    from . import assembly_test

    return _json_payload(assembly_test.compute_precomputed_rotation(_decode_payload(payload_b64)))


@_safe_json
def run_assembly_test_json(payload_b64: str = "") -> str:
    from . import assembly_test

    return _json_payload(assembly_test.run_assembly_test(_decode_payload(payload_b64)))


@_safe_json
def start_async_assembly_json(payload_b64: str = "") -> str:
    from . import assembly_async

    return _json_payload(assembly_async.start_async_assembly(_decode_payload(payload_b64)))


@_safe_json
def poll_assembly_status_json() -> str:
    from . import assembly_async

    return _json_payload(assembly_async.poll_assembly_status())


@_safe_json
def cancel_assembly_json() -> str:
    from . import assembly_async

    return _json_payload(assembly_async.cancel_assembly())


def _json_payload(payload: dict[str, Any]) -> str:
    return json.dumps(payload, ensure_ascii=False, separators=(",", ":"))


def _decode_payload(payload_b64: str) -> dict[str, Any]:
    if not payload_b64:
        return {}
    raw = base64.b64decode(str(payload_b64).encode("ascii")).decode("utf-8")
    payload = json.loads(raw)
    if not isinstance(payload, dict):
        raise ValueError("Panel payload must be a JSON object.")
    return payload


def shutdown() -> None:
    try:
        from . import assembly_async

        assembly_async.shutdown_async_assembly()
    except Exception:
        pass
    controller = get_controller()
    controller.stop()


if os.environ.get("UESA_MCP_REGISTER_ATEXIT", "1").strip().lower() not in {"0", "false", "no", "off"}:
    atexit.register(shutdown)
