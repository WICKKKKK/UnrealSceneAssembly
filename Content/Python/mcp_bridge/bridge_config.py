"""Configuration helpers for the UnrealSceneAssembly MCP bridge."""

from __future__ import annotations

import os
from dataclasses import dataclass, replace


ENV_PREFIX = "UESA_"


def _env_name(name: str) -> str:
    return f"{ENV_PREFIX}{name}"


def _get_str(name: str, default: str) -> str:
    value = os.environ.get(_env_name(name), "").strip()
    return value or default


def _get_int(name: str, default: int) -> int:
    value = os.environ.get(_env_name(name), "").strip()
    if not value:
        return default
    try:
        return int(value)
    except ValueError:
        return default


def _get_float(name: str, default: float) -> float:
    value = os.environ.get(_env_name(name), "").strip()
    if not value:
        return default
    try:
        return float(value)
    except ValueError:
        return default


def _get_bool(name: str, default: bool) -> bool:
    value = os.environ.get(_env_name(name), "").strip().lower()
    if not value:
        return default
    return value in {"1", "true", "yes", "on"}


def _normalize_http_path(path: str) -> str:
    path = path.strip() or "/mcp"
    return path if path.startswith("/") else f"/{path}"


def _normalize_port(port: int, default: int = 8780) -> int:
    try:
        value = int(port)
    except (TypeError, ValueError):
        return default
    return value if 1 <= value <= 65535 else default


@dataclass(frozen=True)
class BridgeConfig:
    mcp_host: str
    mcp_port: int
    mcp_path: str
    bridge_host: str
    bridge_port: int
    request_timeout_seconds: float
    autostart_mcp: bool
    verbose: bool
    plugin_python_dir: str
    server_dir: str
    site_packages_dir: str
    server_script: str

    @property
    def mcp_url(self) -> str:
        return f"http://{self.mcp_host}:{self.mcp_port}{self.mcp_path}"

    @property
    def bridge_address(self) -> str:
        return f"{self.bridge_host}:{self.bridge_port}"


def load_config(plugin_python_dir: str | None = None) -> BridgeConfig:
    if plugin_python_dir is None:
        plugin_python_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    server_dir = os.path.join(plugin_python_dir, "mcp_server")
    site_packages_dir = os.path.join(server_dir, "site-packages")

    return BridgeConfig(
        mcp_host=_get_str("MCP_HOST", "127.0.0.1"),
        mcp_port=_normalize_port(_get_int("MCP_PORT", 8780)),
        mcp_path=_normalize_http_path(_get_str("MCP_PATH", "/mcp")),
        bridge_host=_get_str("BRIDGE_HOST", "127.0.0.1"),
        bridge_port=_normalize_port(_get_int("BRIDGE_PORT", 8766), 8766),
        request_timeout_seconds=_get_float("MCP_REQUEST_TIMEOUT", 30.0),
        autostart_mcp=_get_bool("MCP_AUTO_START", False),
        verbose=_get_bool("MCP_VERBOSE", False),
        plugin_python_dir=plugin_python_dir,
        server_dir=server_dir,
        site_packages_dir=site_packages_dir,
        server_script=os.path.join(server_dir, "server.py"),
    )


def with_mcp_port(config: BridgeConfig, port: int) -> BridgeConfig:
    return replace(config, mcp_port=_normalize_port(port, config.mcp_port))
