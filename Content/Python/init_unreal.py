import atexit
import os
import sys

import unreal


PLUGIN_PYTHON_DIR = os.path.dirname(os.path.abspath(__file__))
if PLUGIN_PYTHON_DIR not in sys.path:
    sys.path.insert(0, PLUGIN_PYTHON_DIR)


def _log(message):
    unreal.log(f"[SceneAssembly] {message}")


def _warn(message):
    unreal.log_warning(f"[SceneAssembly] {message}")


def shutdown_scene_assembly_mcp():
    try:
        from mcp_bridge.controller import shutdown

        shutdown()
    except Exception as exc:
        _warn(f"Failed to shut down MCP runtime: {exc}")


def start_scene_assembly_mcp():
    from mcp_bridge.controller import get_controller, start_json

    controller = get_controller(PLUGIN_PYTHON_DIR)
    result = start_json()
    setattr(unreal, "_scene_assembly_mcp_controller", controller)
    return result


if not getattr(unreal, "_scene_assembly_mcp_atexit_registered", False):
    atexit.register(shutdown_scene_assembly_mcp)
    setattr(unreal, "_scene_assembly_mcp_atexit_registered", True)


try:
    from mcp_bridge.controller import get_controller, initialize

    controller = initialize(PLUGIN_PYTHON_DIR)
    setattr(unreal, "_scene_assembly_mcp_controller", controller)
    if controller.config.autostart_mcp:
        _log(f"MCP autostart requested. Endpoint: {controller.config.mcp_url}")
    else:
        _log("MCP autostart is disabled. Open Window > Scene Assembly MCP to start it.")
except Exception as exc:
    _warn(f"Failed to initialize MCP controller: {exc}")

_log("Python scripts loaded. Run ingest_static_meshes.ingest_static_meshes() manually to ingest StaticMesh thumbnails.")
