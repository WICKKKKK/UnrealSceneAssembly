"""External FastMCP server for UnrealSceneAssembly."""

from __future__ import annotations

import os
import site
import sys
from typing import Any


SERVER_DIR = os.path.dirname(os.path.abspath(__file__))
SITE_PACKAGES_DIR = os.path.join(SERVER_DIR, "site-packages")
if os.path.isdir(SITE_PACKAGES_DIR):
    # Process .pth files from --target installs; pywin32 needs this on Windows.
    site.addsitedir(SITE_PACKAGES_DIR)
if SERVER_DIR not in sys.path:
    sys.path.insert(0, SERVER_DIR)

from bridge_client import BridgeError, bridge_settings, call_bridge

try:
    from mcp.server.fastmcp import FastMCP
except Exception as exc:
    sys.stderr.write(
        "MCP Python SDK is not installed. Install dependencies with: "
        f'python -m pip install -r "{os.path.join(SERVER_DIR, "requirements.txt")}" '
        f'--target "{SITE_PACKAGES_DIR}"\n'
    )
    raise


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


def _normalize_path(path: str) -> str:
    path = path.strip() or "/mcp"
    return path if path.startswith("/") else f"/{path}"


def _server_config() -> dict[str, Any]:
    return {
        "host": _get_str("UESA_MCP_HOST", "127.0.0.1"),
        "port": _get_int("UESA_MCP_PORT", 8780),
        "path": _normalize_path(_get_str("UESA_MCP_PATH", "/mcp")),
    }


CONFIG = _server_config()


def _create_mcp_server():
    kwargs: dict[str, Any] = {
        "instructions": (
            "Tools for communicating with the currently open Unreal Engine project through "
            "the UnrealSceneAssembly editor plugin."
        ),
    }
    if not _FASTMCP_RUN_KWARGS:
        kwargs.update(
            {
                "host": CONFIG["host"],
                "port": CONFIG["port"],
                "streamable_http_path": CONFIG["path"],
                "stateless_http": True,
                "json_response": True,
            }
        )
    return FastMCP("UnrealSceneAssembly", **kwargs)


def _detect_fastmcp_run_kwargs(fastmcp_type) -> dict[str, Any]:
    try:
        import inspect

        parameters = inspect.signature(fastmcp_type.run).parameters
    except Exception:
        parameters = {}

    if "kwargs" not in parameters:
        return {}

    return {
        "host": CONFIG["host"],
        "port": CONFIG["port"],
        "streamable_http_path": CONFIG["path"],
        "stateless_http": True,
        "json_response": True,
    }


_FASTMCP_RUN_KWARGS = _detect_fastmcp_run_kwargs(FastMCP)
mcp = _create_mcp_server()


@mcp.tool()
def unreal_ping() -> dict[str, Any]:
    """Check whether the MCP server can reach the running Unreal editor."""
    return _call_unreal("ping")


@mcp.tool()
def unreal_get_project_info() -> dict[str, Any]:
    """Return basic information about the currently open Unreal project."""
    return _call_unreal("get_project_info")


def _call_unreal(command: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    try:
        return call_bridge(command, params=params)
    except BridgeError as exc:
        return {
            "ok": False,
            "error": str(exc),
            "bridge": bridge_settings(),
        }


def main() -> None:
    print(f"UnrealSceneAssembly MCP server listening at http://{CONFIG['host']}:{CONFIG['port']}{CONFIG['path']}")
    mcp.run(transport="streamable-http", **_FASTMCP_RUN_KWARGS)


if __name__ == "__main__":
    main()
