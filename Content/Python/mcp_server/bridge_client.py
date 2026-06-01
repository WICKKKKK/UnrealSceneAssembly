"""Client for the Unreal-side JSON-line bridge."""

from __future__ import annotations

import json
import os
import socket
import uuid
from typing import Any


class BridgeError(RuntimeError):
    """Base error raised by the bridge client."""


class BridgeUnavailable(BridgeError):
    """Raised when the Unreal-side bridge cannot be reached."""


class BridgeProtocolError(BridgeError):
    """Raised when the bridge returns an invalid response."""


def call_bridge(command: str, params: dict[str, Any] | None = None, timeout: float | None = None) -> dict[str, Any]:
    settings = bridge_settings()
    timeout = timeout if timeout is not None else settings["timeout_seconds"]
    request_id = str(uuid.uuid4())
    payload = {
        "id": request_id,
        "command": command,
        "params": params or {},
    }

    try:
        with socket.create_connection((settings["host"], settings["port"]), timeout=timeout) as sock:
            sock.settimeout(timeout)
            with sock.makefile("rwb") as stream:
                stream.write((json.dumps(payload, ensure_ascii=True) + "\n").encode("utf-8"))
                stream.flush()
                raw_response = stream.readline()
    except OSError as exc:
        raise BridgeUnavailable(f"Unable to connect to Unreal bridge at {settings['host']}:{settings['port']}: {exc}") from exc

    if not raw_response:
        raise BridgeProtocolError("Unreal bridge closed the connection without a response")

    try:
        response = json.loads(raw_response.decode("utf-8"))
    except json.JSONDecodeError as exc:
        raise BridgeProtocolError(f"Unreal bridge returned invalid JSON: {exc}") from exc

    if response.get("id") != request_id:
        raise BridgeProtocolError("Unreal bridge response id did not match the request id")

    status = response.get("status")
    if status == "success":
        result = response.get("result")
        return result if isinstance(result, dict) else {"result": result}
    if status == "error":
        error = response.get("error")
        if isinstance(error, dict):
            message = error.get("message") or str(error)
        else:
            message = str(error)
        raise BridgeError(message)

    raise BridgeProtocolError(f"Unreal bridge returned unknown status: {status}")


def bridge_settings() -> dict[str, Any]:
    return {
        "host": _get_str("UESA_BRIDGE_HOST", "127.0.0.1"),
        "port": _get_int("UESA_BRIDGE_PORT", 8766),
        "timeout_seconds": _get_float("UESA_MCP_REQUEST_TIMEOUT", 30.0),
    }


def _get_str(name: str, default: str) -> str:
    value = os.environ.get(name, "").strip()
    return value or default


def _get_int(name: str, default: int) -> int:
    value = os.environ.get(name, "").strip()
    if not value:
        return default
    try:
        return int(value)
    except ValueError:
        return default


def _get_float(name: str, default: float) -> float:
    value = os.environ.get(name, "").strip()
    if not value:
        return default
    try:
        return float(value)
    except ValueError:
        return default
