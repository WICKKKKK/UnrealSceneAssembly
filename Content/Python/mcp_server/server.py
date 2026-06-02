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


@mcp.tool()
def unreal_spawn_asset(
    asset_path: str,
    location: list[float] | None = None,
    rotation: list[float] | None = None,
    scale: list[float] | None = None,
    label: str | None = None,
) -> dict[str, Any]:
    """Spawn a Content Browser asset into the current level. Rotation is [pitch, yaw, roll]."""
    return _call_unreal(
        "spawn_asset",
        {"asset_path": asset_path, "location": location, "rotation": rotation, "scale": scale, "label": label},
    )


@mcp.tool()
def unreal_spawn_actor(
    actor_class: str,
    location: list[float] | None = None,
    rotation: list[float] | None = None,
    scale: list[float] | None = None,
    label: str | None = None,
) -> dict[str, Any]:
    """Spawn an actor by class name/path, such as StaticMeshActor, PointLight, CameraActor, or a Blueprint class path."""
    return _call_unreal(
        "spawn_actor",
        {"actor_class": actor_class, "location": location, "rotation": rotation, "scale": scale, "label": label},
    )


@mcp.tool()
def unreal_set_actor_transform(
    actor_path: str,
    location: list[float] | None = None,
    rotation: list[float] | None = None,
    scale: list[float] | None = None,
) -> dict[str, Any]:
    """Set any subset of an actor transform using ActorPath as the identifier."""
    return _call_unreal(
        "set_actor_transform",
        {"actor_path": actor_path, "location": location, "rotation": rotation, "scale": scale},
    )


@mcp.tool()
def unreal_set_actor_property(actor_path: str, property_name: str, property_value: Any, target: str | None = None) -> dict[str, Any]:
    """Set an editor property on an actor or nested component target. ActorPath is the identifier."""
    return _call_unreal(
        "set_actor_property",
        {"actor_path": actor_path, "property_name": property_name, "property_value": property_value, "target": target},
    )


@mcp.tool()
def unreal_get_actor_details(actor_path: str) -> dict[str, Any]:
    """Return transform, class, bounds, components, tags, and common asset info for an actor."""
    return _call_unreal("get_actor_details", {"actor_path": actor_path})


@mcp.tool()
def unreal_list_actors(
    filter_class: str | None = None,
    include_bounds: bool = False,
    include_details: bool = False,
    limit: int = 500,
) -> dict[str, Any]:
    """List actors in the current level, returning ActorPath values for later edits."""
    return _call_unreal(
        "list_actors",
        {
            "filter_class": filter_class,
            "include_bounds": include_bounds,
            "include_details": include_details,
            "limit": limit,
        },
    )


@mcp.tool()
def unreal_find_actors(
    name_pattern: str | None = None,
    filter_class: str | None = None,
    tag: str | None = None,
    asset_path: str | None = None,
    include_bounds: bool = False,
    limit: int = 100,
) -> dict[str, Any]:
    """Find actors by label pattern, class, tag, or static mesh asset path."""
    return _call_unreal(
        "find_actors",
        {
            "name_pattern": name_pattern,
            "filter_class": filter_class,
            "tag": tag,
            "asset_path": asset_path,
            "include_bounds": include_bounds,
            "limit": limit,
        },
    )


@mcp.tool()
def unreal_delete_actor(actor_path: str) -> dict[str, Any]:
    """Delete an actor by ActorPath."""
    return _call_unreal("delete_actor", {"actor_path": actor_path})


@mcp.tool()
def unreal_duplicate_actor(actor_path: str, offset: list[float] | None = None, label: str | None = None) -> dict[str, Any]:
    """Duplicate an actor by ActorPath with an optional [x, y, z] offset."""
    return _call_unreal("duplicate_actor", {"actor_path": actor_path, "offset": offset, "label": label})


@mcp.tool()
def unreal_drop_actor_to_floor(
    actor_path: str,
    trace_up: float = 5000.0,
    trace_down: float = 5000.0,
    z_offset: float = 0.0,
    trace_channel: str = "visibility",
    trace_complex: bool = False,
) -> dict[str, Any]:
    """Drop an actor vertically onto the first blocking surface, preserving pivot-to-bottom offset."""
    return _call_unreal(
        "drop_actor_to_floor",
        {
            "actor_path": actor_path,
            "trace_up": trace_up,
            "trace_down": trace_down,
            "z_offset": z_offset,
            "trace_channel": trace_channel,
            "trace_complex": trace_complex,
        },
    )


@mcp.tool()
def unreal_get_actor_bounds(actor_path: str) -> dict[str, Any]:
    """Return world-space bounds for an actor by ActorPath."""
    return _call_unreal("get_actor_bounds", {"actor_path": actor_path})


@mcp.tool()
def unreal_batch_spawn_assets(items: list[dict[str, Any]]) -> dict[str, Any]:
    """Spawn multiple assets in one editor transaction. Each item supports asset_path, location, rotation, scale, label."""
    return _call_unreal("batch_spawn_assets", {"items": items})


@mcp.tool()
def unreal_align_actors(
    actor_paths: list[str],
    axis: str = "x",
    mode: str = "center",
    value: float | None = None,
    start: float | None = None,
    end: float | None = None,
    grid_size: float = 100.0,
) -> dict[str, Any]:
    """Align, distribute, or grid-snap actors. Mode is min, center, max, distribute, or grid."""
    return _call_unreal(
        "align_actors",
        {
            "actor_paths": actor_paths,
            "axis": axis,
            "mode": mode,
            "value": value,
            "start": start,
            "end": end,
            "grid_size": grid_size,
        },
    )


@mcp.tool()
def unreal_set_viewport_camera(location: list[float], rotation: list[float]) -> dict[str, Any]:
    """Set the active level viewport camera. Rotation is [pitch, yaw, roll]."""
    return _call_unreal("set_viewport_camera", {"location": location, "rotation": rotation})


@mcp.tool()
def unreal_get_viewport_camera() -> dict[str, Any]:
    """Return the active level viewport camera location and rotation."""
    return _call_unreal("get_viewport_camera")


@mcp.tool()
def unreal_focus_camera_on_actor(
    actor_path: str,
    distance: float | None = None,
    distance_multiplier: float = 2.5,
    min_distance: float = 300.0,
    pitch: float = -25.0,
    yaw: float = -45.0,
) -> dict[str, Any]:
    """Move the viewport camera to frame an actor's bounds."""
    return _call_unreal(
        "focus_camera_on_actor",
        {
            "actor_path": actor_path,
            "distance": distance,
            "distance_multiplier": distance_multiplier,
            "min_distance": min_distance,
            "pitch": pitch,
            "yaw": yaw,
        },
    )


@mcp.tool()
def unreal_take_screenshot(output_path: str | None = None, width: int = 0, height: int = 0) -> dict[str, Any]:
    """Synchronously save the active editor viewport to a PNG file, optionally resizing to width/height."""
    return _call_unreal("take_screenshot", {"output_path": output_path, "width": width, "height": height})


@mcp.tool()
def unreal_save_level() -> dict[str, Any]:
    """Save the current level."""
    return _call_unreal("save_level")


@mcp.tool()
def unreal_new_level(path: str, save_current: bool = False) -> dict[str, Any]:
    """Create a new level at an Unreal content path such as /Game/Maps/TestMap."""
    return _call_unreal("new_level", {"path": path, "save_current": save_current})


@mcp.tool()
def unreal_load_level(path: str, save_current: bool = False) -> dict[str, Any]:
    """Load a level by Unreal content path such as /Game/Maps/TestMap."""
    return _call_unreal("load_level", {"path": path, "save_current": save_current})


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
