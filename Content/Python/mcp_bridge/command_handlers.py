"""Main-thread Unreal command handlers exposed through the MCP bridge."""

from __future__ import annotations

import datetime as _datetime
import os
import platform
import sys
from typing import Any

import unreal

from .scene_handlers import SCENE_HANDLERS
from .solver_handlers import SOLVER_HANDLERS


class CommandError(RuntimeError):
    """Raised when a bridge command cannot be handled."""


def handle_command(command: str, params: dict[str, Any] | None = None, context: dict[str, Any] | None = None) -> dict[str, Any]:
    params = params or {}
    context = context or {}

    handlers: dict[str, Any] = {
        "ping": _handle_ping,
        "get_project_info": _handle_get_project_info,
    }
    handlers.update(SCENE_HANDLERS)
    handlers.update(SOLVER_HANDLERS)

    handler = handlers.get(command)
    if handler is None:
        raise CommandError(f"Unknown bridge command: {command}")

    return handler(params, context)


def _handle_ping(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    return {
        "ok": True,
        "message": "pong",
        "timestamp_utc": _utc_now(),
        "project": _project_summary(),
        "engine_version": _engine_version(),
        "bridge": _bridge_summary(context),
    }


def _handle_get_project_info(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    config = context.get("config")
    return {
        "ok": True,
        "timestamp_utc": _utc_now(),
        "project": _project_summary(),
        "engine": {
            "version": _engine_version(),
            "dir": _call_unreal_path("engine_dir"),
        },
        "python": {
            "version": sys.version,
            "executable": sys.executable,
            "platform": platform.platform(),
        },
        "plugin": {
            "python_dir": getattr(config, "plugin_python_dir", None),
            "mcp_server_dir": getattr(config, "server_dir", None),
        },
        "mcp": {
            "url": getattr(config, "mcp_url", None),
            "bridge_address": getattr(config, "bridge_address", None),
        },
    }


def _project_summary() -> dict[str, Any]:
    project_file = _absolute_unreal_path(_call_first(unreal.Paths, ("get_project_file_path", "project_file_path")))
    project_dir = _call_unreal_path("project_dir")
    content_dir = _call_unreal_path("project_content_dir")
    project_name = _call_first(unreal.SystemLibrary, ("get_project_name",))

    if not project_name:
        source = project_file or project_dir or ""
        source = source.rstrip("/\\")
        project_name = os.path.splitext(os.path.basename(source))[0] if source else None

    return {
        "name": project_name,
        "file": project_file,
        "dir": project_dir,
        "content_dir": content_dir,
    }


def _engine_version() -> str | None:
    return _call_first(unreal.SystemLibrary, ("get_engine_version",))


def _bridge_summary(context: dict[str, Any]) -> dict[str, Any]:
    config = context.get("config")
    return {
        "address": getattr(config, "bridge_address", None),
        "request_timeout_seconds": getattr(config, "request_timeout_seconds", None),
    }


def _call_unreal_path(name: str) -> str | None:
    return _absolute_unreal_path(_call_first(unreal.Paths, (name,)))


def _absolute_unreal_path(path: Any) -> str | None:
    if not path:
        return None
    path = str(path)
    converter = getattr(unreal.Paths, "convert_relative_path_to_full", None)
    if callable(converter):
        try:
            path = str(converter(path))
        except Exception:
            pass
    return os.path.abspath(path)


def _call_first(owner: Any, names: tuple[str, ...]) -> Any:
    for name in names:
        func = getattr(owner, name, None)
        if callable(func):
            try:
                return func()
            except Exception:
                continue
    return None



def _utc_now() -> str:
    return _datetime.datetime.now(_datetime.timezone.utc).isoformat().replace("+00:00", "Z")
