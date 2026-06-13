"""Scene editing command handlers for UnrealSceneAssembly MCP."""

from __future__ import annotations

import fnmatch
import math
import os
import time
from typing import Any, Callable

import unreal


_MISSING = object()


def _asset_path_no_suffix(path: Any) -> str:
    value = str(path or "").strip()
    if not value or value == "None":
        return ""
    slash_index = value.rfind("/")
    dot_index = value.find(".", slash_index + 1)
    return value[:dot_index] if dot_index >= 0 else value


def _object_path_from_asset_path(path: Any) -> str:
    value = str(path or "").strip()
    if not value or value == "None":
        return ""
    slash_index = value.rfind("/")
    if value.find(".", slash_index + 1) >= 0:
        return value
    asset_name = value[slash_index + 1 :] if slash_index >= 0 else value
    return f"{value}.{asset_name}" if asset_name else value


class SceneCommandError(RuntimeError):
    """Raised when a scene command receives invalid input."""


def _safe(handler: Callable[[dict[str, Any], dict[str, Any]], dict[str, Any]]) -> Callable[[dict[str, Any], dict[str, Any]], dict[str, Any]]:
    def wrapped(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
        try:
            return handler(params or {}, context or {})
        except SceneCommandError as exc:
            return _error(str(exc))
        except Exception as exc:
            return _error(str(exc), type=type(exc).__name__)

    return wrapped


def _handle_spawn_asset(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    with unreal.ScopedEditorTransaction("Scene Assembly: Spawn Asset"):
        actor = _spawn_asset_no_transaction(params)
    return _success(actor=_actor_summary(actor, include_bounds=True))


def _handle_spawn_actor(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor_class_name = _require_str(params, "actor_class")
    actor_class = _resolve_actor_class(actor_class_name)
    location = _to_vector(params.get("location"), unreal.Vector(0.0, 0.0, 0.0))
    rotation = _to_rotator(params.get("rotation"), _zero_rotator())
    scale = _to_vector(params.get("scale"), None)

    with unreal.ScopedEditorTransaction("Scene Assembly: Spawn Actor"):
        actor = _spawn_from_class(actor_class, location, rotation)
        if not actor:
            raise SceneCommandError(f"Failed to spawn actor class: {actor_class_name}")
        _set_label_if_requested(actor, params.get("label"))
        if scale is not None:
            actor.set_actor_scale3d(scale)

    return _success(actor=_actor_summary(actor, include_bounds=True))


def _handle_set_actor_transform(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    changed: list[str] = []

    with unreal.ScopedEditorTransaction("Scene Assembly: Set Actor Transform"):
        _modify(actor)
        if "location" in params and params.get("location") is not None:
            actor.set_actor_location(_to_vector(params.get("location")), False, False)
            changed.append("location")
        if "rotation" in params and params.get("rotation") is not None:
            actor.set_actor_rotation(_to_rotator(params.get("rotation")), False)
            changed.append("rotation")
        if "scale" in params and params.get("scale") is not None:
            actor.set_actor_scale3d(_to_vector(params.get("scale")))
            changed.append("scale")

    if not changed:
        return _success(message="No transform fields were provided.", actor=_actor_summary(actor, include_bounds=True))
    return _success(changed=changed, actor=_actor_summary(actor, include_bounds=True))


def _handle_set_actor_property(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    property_name = _require_str(params, "property_name")
    target, resolved_property_name = _resolve_property_target(actor, property_name, params.get("target"))
    value = params.get("property_value") if "property_value" in params else params.get("value")

    try:
        current_value = target.get_editor_property(resolved_property_name)
        converted_value = _convert_property_value(current_value, value)
    except Exception:
        converted_value = value
    if converted_value is value and isinstance(value, str) and value.startswith(("/Game/", "/Script/")):
        converted_value = _try_load_asset(value) or value

    with unreal.ScopedEditorTransaction("Scene Assembly: Set Actor Property"):
        _modify(target)
        target.set_editor_property(resolved_property_name, converted_value)

    try:
        new_value = _serialize_unreal_value(target.get_editor_property(resolved_property_name))
    except Exception:
        new_value = _serialize_unreal_value(converted_value)

    return _success(
        actor=_actor_summary(actor),
        property_name=resolved_property_name,
        target=_target_name(target),
        value=new_value,
    )


def _handle_get_actor_details(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    return _success(actor=_actor_summary(_actor_from_params(params), include_bounds=True, include_details=True))


def _handle_list_actors(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    class_filter = params.get("filter_class") or params.get("class_filter")
    include_bounds = bool(params.get("include_bounds", False))
    include_details = bool(params.get("include_details", False))
    limit = int(params.get("limit", 500))

    actors = []
    for actor in _editor_actor_subsystem().get_all_level_actors():
        if actor and _matches_class(actor, class_filter):
            actors.append(_actor_summary(actor, include_bounds=include_bounds, include_details=include_details))
            if limit > 0 and len(actors) >= limit:
                break

    return _success(actors=actors, count=len(actors), limit=limit)


def _handle_find_actors(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    pattern = params.get("name_pattern") or params.get("label_pattern") or params.get("pattern")
    class_filter = params.get("filter_class") or params.get("class_filter")
    tag = params.get("tag")
    asset_path = params.get("asset_path")
    limit = int(params.get("limit", 100))

    matches = []
    for actor in _editor_actor_subsystem().get_all_level_actors():
        if not actor:
            continue
        if pattern and not _matches_pattern(actor.get_actor_label(), str(pattern)):
            continue
        if not _matches_class(actor, class_filter):
            continue
        if tag and str(tag) not in _actor_tags(actor):
            continue
        if asset_path and _actor_asset_path(actor) != _asset_path_no_suffix(asset_path):
            continue
        matches.append(_actor_summary(actor, include_bounds=bool(params.get("include_bounds", False))))
        if limit > 0 and len(matches) >= limit:
            break

    return _success(actors=matches, count=len(matches), limit=limit)


def _handle_delete_actor(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    summary = _actor_summary(actor)
    with unreal.ScopedEditorTransaction("Scene Assembly: Delete Actor"):
        deleted = bool(_editor_actor_subsystem().destroy_actor(actor))
    if not deleted:
        return _error(f"Failed to delete actor: {summary['actor_path']}", actor=summary)
    return _success(deleted=True, actor=summary)


def _handle_duplicate_actor(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    offset = _to_vector(params.get("offset"), unreal.Vector(0.0, 0.0, 0.0))

    with unreal.ScopedEditorTransaction("Scene Assembly: Duplicate Actor"):
        duplicate = _editor_actor_subsystem().duplicate_actor(actor, offset=offset)
        if not duplicate:
            raise SceneCommandError(f"Failed to duplicate actor: {actor.get_path_name()}")
        _set_label_if_requested(duplicate, params.get("label"))

    return _success(source=_actor_summary(actor), actor=_actor_summary(duplicate, include_bounds=True))


def _handle_drop_actor_to_floor(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    trace_up = float(params.get("trace_up", 5000.0))
    trace_down = float(params.get("trace_down", 5000.0))
    z_offset = float(params.get("z_offset", 0.0))
    trace_complex = bool(params.get("trace_complex", False))

    location = actor.get_actor_location()
    origin, extent = actor.get_actor_bounds(False)
    bottom_z = origin.z - extent.z
    pivot_to_bottom = location.z - bottom_z
    start = unreal.Vector(origin.x, origin.y, max(origin.z + extent.z, location.z) + trace_up)
    end = unreal.Vector(origin.x, origin.y, min(origin.z - extent.z, location.z) - trace_down)

    hit = unreal.SystemLibrary.line_trace_single(
        world_context_object=_editor_world(),
        start=start,
        end=end,
        trace_channel=_trace_channel(params.get("trace_channel")),
        trace_complex=trace_complex,
        actors_to_ignore=[actor],
        draw_debug_type=unreal.DrawDebugTrace.NONE,
        ignore_self=True,
    )
    hit_info = _hit_result_info(hit)
    if not hit_info.get("hit"):
        return _error("Drop-to-floor trace did not hit a blocking surface.", trace=hit_info)

    impact = hit_info["impact_point"]
    new_location = unreal.Vector(location.x, location.y, float(impact[2]) + pivot_to_bottom + z_offset)
    with unreal.ScopedEditorTransaction("Scene Assembly: Drop Actor To Floor"):
        _modify(actor)
        actor.set_actor_location(new_location, False, False)

    return _success(actor=_actor_summary(actor, include_bounds=True), trace=hit_info)


def _handle_get_actor_bounds(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    return _success(actor=_actor_summary(actor), bounds=_actor_bounds(actor))


def _handle_batch_spawn_assets(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    items = params.get("items")
    if not isinstance(items, list) or not items:
        raise SceneCommandError("Parameter 'items' must be a non-empty list.")

    spawned = []
    errors = []
    with unreal.ScopedEditorTransaction("Scene Assembly: Batch Spawn Assets"):
        for index, item in enumerate(items):
            if not isinstance(item, dict):
                errors.append({"index": index, "message": "Item must be an object."})
                continue
            try:
                actor = _spawn_asset_no_transaction(item)
                spawned.append(_actor_summary(actor, include_bounds=True))
            except Exception as exc:
                errors.append({"index": index, "message": str(exc)})

    if not spawned and errors:
        return _error("No assets were spawned.", errors=errors)
    return _success(spawned=spawned, count=len(spawned), errors=errors)


def _handle_align_actors(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actors = _actors_from_params(params)
    mode = str(params.get("mode", "center")).lower()
    axis = str(params.get("axis", "x")).lower()

    with unreal.ScopedEditorTransaction("Scene Assembly: Align Actors"):
        if mode in {"grid", "snap", "snap_grid"}:
            updated = _snap_actors_to_grid(actors, axis, float(params.get("grid_size", 100.0)))
        elif mode == "distribute":
            updated = _distribute_actors(actors, axis, params)
        elif mode in {"min", "center", "max"}:
            updated = _align_actors_to_bound(actors, axis, mode, params.get("value"))
        else:
            raise SceneCommandError("Parameter 'mode' must be one of: min, center, max, distribute, grid.")

    return _success(actors=updated, count=len(updated), mode=mode, axis=axis)


def _handle_set_viewport_camera(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    location = _to_vector(params.get("location"))
    rotation = _to_rotator(params.get("rotation"))
    _unreal_editor_subsystem().set_level_viewport_camera_info(location, rotation)
    return _success(camera=_camera_summary(location, rotation))


def _handle_get_viewport_camera(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    location, rotation = _unreal_editor_subsystem().get_level_viewport_camera_info()
    return _success(camera=_camera_summary(location, rotation))


def _handle_focus_camera_on_actor(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    origin, extent = actor.get_actor_bounds(False)
    radius = max(_vector_length(extent), 1.0)
    distance = params.get("distance")
    if distance is None:
        distance = max(radius * float(params.get("distance_multiplier", 2.5)), float(params.get("min_distance", 300.0)))
    pitch = float(params.get("pitch", -25.0))
    yaw = float(params.get("yaw", -45.0))
    direction = _direction_from_pitch_yaw(pitch, yaw)
    location = unreal.Vector(
        origin.x - direction.x * float(distance),
        origin.y - direction.y * float(distance),
        origin.z - direction.z * float(distance),
    )
    rotation = _look_at_rotation(location, origin)
    _unreal_editor_subsystem().set_level_viewport_camera_info(location, rotation)
    return _success(actor=_actor_summary(actor), camera=_camera_summary(location, rotation))


def _handle_take_screenshot(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    output_path = params.get("output_path") or params.get("path") or _default_screenshot_path()
    output_path = _normalize_png_path(str(output_path))
    width = int(params.get("width") or 0)
    height = int(params.get("height") or 0)

    capture_library = getattr(unreal, "ViewportCaptureLibrary", None)
    if capture_library is None or not hasattr(capture_library, "capture_active_viewport"):
        return _error("ViewportCaptureLibrary is unavailable. Recompile and reload the UnrealSceneAssembly plugin.")

    success = bool(capture_library.capture_active_viewport(output_path, width, height))
    if not success:
        return _error("Failed to capture the active editor viewport.", output_path=output_path)
    return _success(output_path=output_path, width=width or None, height=height or None, exists=os.path.exists(output_path))


def _handle_save_level(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    success = bool(_level_editor_subsystem().save_current_level())
    if not success:
        return _error("Failed to save the current level.")
    return _success(saved=True)


def _handle_new_level(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    level_path = _require_str(params, "path")
    if bool(params.get("save_current", False)):
        _level_editor_subsystem().save_current_level()
    success = bool(_level_editor_subsystem().new_level(level_path))
    if not success:
        return _error(f"Failed to create new level: {level_path}")
    return _success(path=level_path)


def _handle_load_level(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    level_path = _require_str(params, "path")
    if bool(params.get("save_current", False)):
        _level_editor_subsystem().save_current_level()
    success = bool(_level_editor_subsystem().load_level(level_path))
    if not success:
        return _error(f"Failed to load level: {level_path}")
    return _success(path=level_path)


def _spawn_asset_no_transaction(params: dict[str, Any]) -> Any:
    asset_path = _require_str(params, "asset_path")
    asset = _load_asset(asset_path)
    location = _to_vector(params.get("location"), unreal.Vector(0.0, 0.0, 0.0))
    rotation = _to_rotator(params.get("rotation"), _zero_rotator())
    scale = _to_vector(params.get("scale"), None)

    actor = _spawn_from_object(asset, location, rotation)
    if not actor:
        raise SceneCommandError(f"Failed to spawn asset: {asset_path}")
    _set_label_if_requested(actor, params.get("label"))
    _set_tags_if_requested(actor, params.get("tags"))
    if scale is not None:
        actor.set_actor_scale3d(scale)
    return actor


def _spawn_from_object(asset: Any, location: Any, rotation: Any) -> Any:
    subsystem = _editor_actor_subsystem()
    try:
        return subsystem.spawn_actor_from_object(asset, location, rotation)
    except TypeError:
        actor = subsystem.spawn_actor_from_object(asset, location)
        if actor:
            actor.set_actor_rotation(rotation, False)
        return actor


def _spawn_from_class(actor_class: Any, location: Any, rotation: Any) -> Any:
    subsystem = _editor_actor_subsystem()
    try:
        return subsystem.spawn_actor_from_class(actor_class, location, rotation)
    except TypeError:
        actor = subsystem.spawn_actor_from_class(actor_class, location)
        if actor:
            actor.set_actor_rotation(rotation, False)
        return actor


def _resolve_actor_class(actor_class_name: str) -> Any:
    aliases = {
        "actor": getattr(unreal, "Actor", None),
        "staticmeshactor": getattr(unreal, "StaticMeshActor", None),
        "static_mesh_actor": getattr(unreal, "StaticMeshActor", None),
        "pointlight": getattr(unreal, "PointLight", None),
        "point_light": getattr(unreal, "PointLight", None),
        "spotlight": getattr(unreal, "SpotLight", None),
        "spot_light": getattr(unreal, "SpotLight", None),
        "directionallight": getattr(unreal, "DirectionalLight", None),
        "directional_light": getattr(unreal, "DirectionalLight", None),
        "cameraactor": getattr(unreal, "CameraActor", None),
        "camera_actor": getattr(unreal, "CameraActor", None),
        "cinecameraactor": getattr(unreal, "CineCameraActor", None),
        "cine_camera_actor": getattr(unreal, "CineCameraActor", None),
    }
    key = actor_class_name.strip().replace(" ", "").lower()
    if aliases.get(key):
        return aliases[key]

    direct = getattr(unreal, actor_class_name.strip(), None)
    if direct:
        return direct

    for class_path in _candidate_class_paths(actor_class_name):
        actor_class = unreal.load_class(None, class_path)
        if actor_class:
            return actor_class

    asset = _try_load_asset(actor_class_name)
    if asset:
        generated_class = getattr(asset, "generated_class", None)
        if generated_class:
            return generated_class
        getter = getattr(asset, "get_generated_class", None)
        if callable(getter):
            generated_class = getter()
            if generated_class:
                return generated_class

    raise SceneCommandError(f"Failed to resolve actor class: {actor_class_name}")


def _candidate_class_paths(actor_class_name: str) -> list[str]:
    name = actor_class_name.strip()
    candidates = [name]
    if name.startswith("/Game/") and not name.endswith("_C"):
        candidates.append(f"{name}_C")
    if not name.startswith("/") and "." not in name:
        candidates.append(f"/Script/Engine.{name}")
    return candidates


def _load_asset(asset_path: str) -> Any:
    asset = _try_load_asset(asset_path)
    if not asset:
        raise SceneCommandError(f"Failed to load asset: {asset_path}")
    return asset


def _try_load_asset(asset_path: str) -> Any:
    candidates = []
    for candidate in (asset_path, _object_path_from_asset_path(asset_path)):
        if candidate and candidate not in candidates:
            candidates.append(candidate)

    for candidate in candidates:
        try:
            asset = unreal.EditorAssetLibrary.load_asset(candidate)
            if asset:
                return asset
        except Exception:
            pass
    for candidate in candidates:
        try:
            asset = unreal.load_asset(candidate)
            if asset:
                return asset
        except Exception:
            pass
    return None


def _actor_from_params(params: dict[str, Any]) -> Any:
    return _find_actor_by_path(_require_str(params, "actor_path"))


def _actors_from_params(params: dict[str, Any]) -> list[Any]:
    actor_paths = params.get("actor_paths")
    if not isinstance(actor_paths, list) or not actor_paths:
        raise SceneCommandError("Parameter 'actor_paths' must be a non-empty list.")
    return [_find_actor_by_path(str(path)) for path in actor_paths]


def _find_actor_by_path(actor_path: str) -> Any:
    actor_path = actor_path.strip()
    if not actor_path:
        raise SceneCommandError("Parameter 'actor_path' must be a non-empty string.")

    try:
        actor = unreal.find_object(None, actor_path)
        if actor and hasattr(actor, "get_path_name") and actor.get_path_name() == actor_path:
            return actor
    except Exception:
        pass

    for actor in _editor_actor_subsystem().get_all_level_actors():
        if actor and actor.get_path_name() == actor_path:
            return actor

    raise SceneCommandError(f"Actor not found by actor_path: {actor_path}")


def _resolve_property_target(actor: Any, property_name: str, target: Any) -> tuple[Any, str]:
    current = actor
    if target:
        current = _resolve_object_segment(actor, str(target))
    parts = [part for part in property_name.split(".") if part]
    if not parts:
        raise SceneCommandError("Parameter 'property_name' must be a non-empty string.")
    for segment in parts[:-1]:
        current = _resolve_object_segment(current, segment)
    return current, parts[-1]


def _resolve_object_segment(owner: Any, segment: str) -> Any:
    if hasattr(owner, segment):
        value = getattr(owner, segment)
        if value is not None:
            return value
    getter = getattr(owner, "get_editor_property", None)
    if callable(getter):
        try:
            value = getter(segment)
            if value is not None:
                return value
        except Exception:
            pass
    if hasattr(owner, "get_components_by_class"):
        for component in owner.get_components_by_class(unreal.ActorComponent):
            if component and segment in {component.get_name(), _target_name(component)}:
                return component
    raise SceneCommandError(f"Unable to resolve property target segment: {segment}")


def _convert_property_value(current_value: Any, value: Any) -> Any:
    if isinstance(current_value, unreal.Vector):
        return _to_vector(value)
    if isinstance(current_value, unreal.Rotator):
        return _to_rotator(value)
    if hasattr(unreal, "LinearColor") and isinstance(current_value, unreal.LinearColor):
        return _to_linear_color(value)
    if hasattr(unreal, "Color") and isinstance(current_value, unreal.Color):
        color = _to_color_list(value, 4)
        return unreal.Color(int(color[0]), int(color[1]), int(color[2]), int(color[3]))
    if hasattr(unreal, "Name") and isinstance(current_value, unreal.Name):
        return unreal.Name(str(value))
    if isinstance(current_value, bool):
        return bool(value)
    if isinstance(current_value, int):
        return int(value)
    if isinstance(current_value, float):
        return float(value)
    if isinstance(current_value, str):
        return str(value)
    if current_value is not None and hasattr(current_value, "get_class") and isinstance(value, str):
        loaded_value = _try_load_asset(value)
        if loaded_value:
            return loaded_value
    return value


def _to_vector(value: Any, default: Any = _MISSING) -> Any:
    if value is None:
        if default is not _MISSING:
            return default
        raise SceneCommandError("Expected a vector as [x, y, z] or {x, y, z}.")
    if isinstance(value, unreal.Vector):
        return value
    if isinstance(value, dict):
        return unreal.Vector(float(value["x"]), float(value["y"]), float(value["z"]))
    if isinstance(value, (list, tuple)) and len(value) == 3:
        return unreal.Vector(float(value[0]), float(value[1]), float(value[2]))
    raise SceneCommandError("Expected a vector as [x, y, z] or {x, y, z}.")


def _to_rotator(value: Any, default: Any = _MISSING) -> Any:
    if value is None:
        if default is not _MISSING:
            return default
        raise SceneCommandError("Expected a rotation as [pitch, yaw, roll] or {pitch, yaw, roll}.")
    if isinstance(value, unreal.Rotator):
        return value
    if isinstance(value, dict):
        return unreal.Rotator(
            roll=float(value.get("roll", 0.0)),
            pitch=float(value.get("pitch", 0.0)),
            yaw=float(value.get("yaw", 0.0)),
        )
    if isinstance(value, (list, tuple)) and len(value) == 3:
        return unreal.Rotator(roll=float(value[2]), pitch=float(value[0]), yaw=float(value[1]))
    raise SceneCommandError("Expected a rotation as [pitch, yaw, roll] or {pitch, yaw, roll}.")


def _to_linear_color(value: Any) -> Any:
    color = _to_color_list(value, 4)
    return unreal.LinearColor(float(color[0]), float(color[1]), float(color[2]), float(color[3]))


def _to_color_list(value: Any, count: int) -> list[float]:
    if isinstance(value, dict):
        keys = ["r", "g", "b", "a"][:count]
        return [float(value[key]) for key in keys]
    if isinstance(value, (list, tuple)) and len(value) == count:
        return [float(item) for item in value]
    raise SceneCommandError(f"Expected a color with {count} channels.")


def _zero_rotator() -> Any:
    return unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)


def _actor_summary(actor: Any, include_bounds: bool = False, include_details: bool = False) -> dict[str, Any]:
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    summary: dict[str, Any] = {
        "actor_path": actor.get_path_name(),
        "label": actor.get_actor_label(),
        "class": _class_path(actor),
        "location": _vector_list(location),
        "rotation": _rotator_list(rotation),
        "scale": _vector_list(scale),
    }
    asset_path = _actor_asset_path(actor)
    if asset_path:
        summary["asset_path"] = asset_path
    if include_bounds:
        summary["bounds"] = _actor_bounds(actor)
    if include_details:
        summary["tags"] = _actor_tags(actor)
        summary["hidden"] = bool(actor.is_hidden_ed()) if hasattr(actor, "is_hidden_ed") else None
        summary["components"] = _component_summaries(actor)
    return summary


def _component_summaries(actor: Any) -> list[dict[str, Any]]:
    components = []
    try:
        for component in actor.get_components_by_class(unreal.ActorComponent):
            if not component:
                continue
            components.append(
                {
                    "name": component.get_name(),
                    "class": component.get_class().get_path_name(),
                    "path": component.get_path_name(),
                }
            )
    except Exception:
        pass
    return components


def _actor_bounds(actor: Any) -> dict[str, Any]:
    origin, extent = actor.get_actor_bounds(False)
    minimum = unreal.Vector(origin.x - extent.x, origin.y - extent.y, origin.z - extent.z)
    maximum = unreal.Vector(origin.x + extent.x, origin.y + extent.y, origin.z + extent.z)
    return {
        "origin": _vector_list(origin),
        "extent": _vector_list(extent),
        "min": _vector_list(minimum),
        "max": _vector_list(maximum),
        "size": [extent.x * 2.0, extent.y * 2.0, extent.z * 2.0],
    }


def _actor_asset_path(actor: Any) -> str | None:
    component = None
    try:
        if hasattr(actor, "get_static_mesh_component"):
            component = actor.get_static_mesh_component()
        elif hasattr(actor, "static_mesh_component"):
            component = actor.static_mesh_component
    except Exception:
        component = None
    if not component:
        return None
    try:
        mesh = component.get_editor_property("static_mesh")
    except Exception:
        mesh = getattr(component, "static_mesh", None)
    return _asset_path_no_suffix(mesh.get_path_name()) if mesh and hasattr(mesh, "get_path_name") else None


def _actor_tags(actor: Any) -> list[str]:
    try:
        return [str(tag) for tag in actor.get_editor_property("tags")]
    except Exception:
        return [str(tag) for tag in getattr(actor, "tags", [])]


def _hit_result_info(hit: Any) -> dict[str, Any]:
    if not hit:
        return {"hit": False}
    values = hit.to_tuple()
    blocking_hit = bool(values[0])
    if not blocking_hit:
        return {"hit": False, "blocking_hit": False}
    location = values[4]
    impact_point = values[5]
    normal = values[6]
    impact_normal = values[7]
    hit_actor = values[9]
    return {
        "hit": True,
        "blocking_hit": blocking_hit,
        "distance": float(values[3]),
        "location": _vector_list(location),
        "impact_point": _vector_list(impact_point),
        "normal": _vector_list(normal),
        "impact_normal": _vector_list(impact_normal),
        "hit_actor_path": hit_actor.get_path_name() if hit_actor else None,
        "hit_actor_label": hit_actor.get_actor_label() if hit_actor else None,
    }


def _align_actors_to_bound(actors: list[Any], axis: str, mode: str, value: Any) -> list[dict[str, Any]]:
    axis_index = _axis_index(axis)
    target = float(value) if value is not None else _bound_coordinate(_actor_bounds(actors[0]), axis_index, mode)
    updated = []
    for actor in actors:
        bounds = _actor_bounds(actor)
        current = _bound_coordinate(bounds, axis_index, mode)
        delta = target - current
        _move_actor_axis(actor, axis_index, delta)
        updated.append(_actor_summary(actor, include_bounds=True))
    return updated


def _distribute_actors(actors: list[Any], axis: str, params: dict[str, Any]) -> list[dict[str, Any]]:
    if len(actors) < 2:
        raise SceneCommandError("Distribute mode requires at least two actors.")
    axis_index = _axis_index(axis)
    sorted_actors = sorted(actors, key=lambda actor: _bound_coordinate(_actor_bounds(actor), axis_index, "center"))
    start_value = params.get("start")
    end_value = params.get("end")
    if start_value is None:
        start_value = _bound_coordinate(_actor_bounds(sorted_actors[0]), axis_index, "center")
    if end_value is None:
        end_value = _bound_coordinate(_actor_bounds(sorted_actors[-1]), axis_index, "center")
    step = (float(end_value) - float(start_value)) / float(len(sorted_actors) - 1)
    updated = []
    for index, actor in enumerate(sorted_actors):
        current = _bound_coordinate(_actor_bounds(actor), axis_index, "center")
        target = float(start_value) + step * float(index)
        _move_actor_axis(actor, axis_index, target - current)
        updated.append(_actor_summary(actor, include_bounds=True))
    return updated


def _snap_actors_to_grid(actors: list[Any], axis: str, grid_size: float) -> list[dict[str, Any]]:
    if grid_size <= 0.0:
        raise SceneCommandError("Parameter 'grid_size' must be greater than 0.")
    axes = [0, 1, 2] if axis in {"all", "xyz", "*"} else [_axis_index(axis)]
    updated = []
    for actor in actors:
        location = actor.get_actor_location()
        values = [location.x, location.y, location.z]
        for axis_index in axes:
            values[axis_index] = round(values[axis_index] / grid_size) * grid_size
        _modify(actor)
        actor.set_actor_location(unreal.Vector(values[0], values[1], values[2]), False, False)
        updated.append(_actor_summary(actor, include_bounds=True))
    return updated


def _move_actor_axis(actor: Any, axis_index: int, delta: float) -> None:
    location = actor.get_actor_location()
    values = [location.x, location.y, location.z]
    values[axis_index] += delta
    _modify(actor)
    actor.set_actor_location(unreal.Vector(values[0], values[1], values[2]), False, False)


def _bound_coordinate(bounds: dict[str, Any], axis_index: int, mode: str) -> float:
    if mode == "min":
        return float(bounds["min"][axis_index])
    if mode == "max":
        return float(bounds["max"][axis_index])
    return float(bounds["origin"][axis_index])


def _axis_index(axis: str) -> int:
    normalized = str(axis).lower()
    if normalized == "x":
        return 0
    if normalized == "y":
        return 1
    if normalized == "z":
        return 2
    raise SceneCommandError("Parameter 'axis' must be x, y, z, or all for grid snapping.")


def _camera_summary(location: Any, rotation: Any) -> dict[str, Any]:
    return {"location": _vector_list(location), "rotation": _rotator_list(rotation, normalize=True)}


def _direction_from_pitch_yaw(pitch: float, yaw: float) -> Any:
    pitch_radians = math.radians(pitch)
    yaw_radians = math.radians(yaw)
    return unreal.Vector(
        math.cos(pitch_radians) * math.cos(yaw_radians),
        math.cos(pitch_radians) * math.sin(yaw_radians),
        math.sin(pitch_radians),
    )


def _look_at_rotation(location: Any, target: Any) -> Any:
    delta_x = target.x - location.x
    delta_y = target.y - location.y
    delta_z = target.z - location.z
    horizontal = math.sqrt(delta_x * delta_x + delta_y * delta_y)
    yaw = math.degrees(math.atan2(delta_y, delta_x))
    pitch = math.degrees(math.atan2(delta_z, horizontal))
    return unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)


def _trace_channel(value: Any) -> Any:
    channel = str(value or "visibility").lower()
    if channel == "camera":
        return unreal.TraceTypeQuery.TRACE_TYPE_QUERY2
    return unreal.TraceTypeQuery.TRACE_TYPE_QUERY1


def _vector_length(vector: Any) -> float:
    try:
        return float(vector.length())
    except Exception:
        return math.sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z)


def _vector_list(vector: Any) -> list[float]:
    return [float(vector.x), float(vector.y), float(vector.z)]


def _rotator_list(rotator: Any, normalize: bool = False) -> list[float]:
    values = [float(rotator.pitch), float(rotator.yaw), float(rotator.roll)]
    if normalize:
        return [_normalize_angle_degrees(value) for value in values]
    return values


def _normalize_angle_degrees(angle: float) -> float:
    normalized = (float(angle) + 180.0) % 360.0 - 180.0
    if normalized == -180.0 and angle > 0.0:
        normalized = 180.0
    return 0.0 if abs(normalized) < 1.0e-9 else normalized


def _class_path(actor: Any) -> str:
    try:
        return actor.get_class().get_path_name()
    except Exception:
        return type(actor).__name__


def _matches_class(actor: Any, class_filter: Any) -> bool:
    if not class_filter:
        return True
    needle = str(class_filter).lower()
    class_path = _class_path(actor).lower()
    class_name = class_path.rsplit(".", 1)[-1].lower()
    return needle in {class_name, class_path} or needle in class_path


def _matches_pattern(value: str, pattern: str) -> bool:
    value_lower = value.lower()
    pattern_lower = pattern.lower()
    if any(char in pattern_lower for char in "*?"):
        return fnmatch.fnmatch(value_lower, pattern_lower)
    return pattern_lower in value_lower


def _set_label_if_requested(actor: Any, label: Any) -> None:
    if label is not None and str(label).strip():
        actor.set_actor_label(str(label).strip())


def _set_tags_if_requested(actor: Any, tags: Any) -> None:
    if tags is None:
        return
    if isinstance(tags, str):
        values = [item.strip() for item in tags.split(",") if item.strip()]
    elif isinstance(tags, (list, tuple)):
        values = [str(item).strip() for item in tags if str(item).strip()]
    else:
        raise SceneCommandError("Parameter 'tags' must be a string or list of strings.")
    if not values:
        return

    merged = _actor_tags(actor)
    for value in values:
        if value not in merged:
            merged.append(value)
    _modify(actor)
    actor.set_editor_property("tags", merged)


def _modify(obj: Any) -> None:
    modifier = getattr(obj, "modify", None)
    if callable(modifier):
        modifier()


def _serialize_unreal_value(value: Any) -> Any:
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Vector):
        return _vector_list(value)
    if isinstance(value, unreal.Rotator):
        return _rotator_list(value)
    if hasattr(unreal, "LinearColor") and isinstance(value, unreal.LinearColor):
        return [float(value.r), float(value.g), float(value.b), float(value.a)]
    if hasattr(unreal, "Color") and isinstance(value, unreal.Color):
        return [int(value.r), int(value.g), int(value.b), int(value.a)]
    if isinstance(value, (list, tuple)):
        return [_serialize_unreal_value(item) for item in value]
    if isinstance(value, dict):
        return {str(key): _serialize_unreal_value(item) for key, item in value.items()}
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)


def _target_name(target: Any) -> str:
    try:
        return target.get_name()
    except Exception:
        return type(target).__name__


def _require_str(params: dict[str, Any], key: str) -> str:
    value = params.get(key)
    if value is None or not str(value).strip():
        raise SceneCommandError(f"Parameter '{key}' must be a non-empty string.")
    return str(value).strip()


def _default_screenshot_path() -> str:
    saved_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    output_dir = os.path.join(saved_dir, "SceneAssembly", "Screenshots")
    filename = f"viewport_{time.strftime('%Y%m%d_%H%M%S')}.png"
    return os.path.abspath(os.path.join(output_dir, filename))


def _normalize_png_path(path: str) -> str:
    normalized = os.path.abspath(path)
    if not normalized.lower().endswith(".png"):
        normalized = f"{normalized}.png"
    return normalized


def _editor_actor_subsystem() -> Any:
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not subsystem:
        raise SceneCommandError("EditorActorSubsystem is unavailable.")
    return subsystem


def _unreal_editor_subsystem() -> Any:
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if not subsystem:
        raise SceneCommandError("UnrealEditorSubsystem is unavailable.")
    return subsystem


def _level_editor_subsystem() -> Any:
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem:
        raise SceneCommandError("LevelEditorSubsystem is unavailable.")
    return subsystem


def _editor_world() -> Any:
    world = _unreal_editor_subsystem().get_editor_world()
    if not world:
        raise SceneCommandError("Editor world is unavailable.")
    return world


def _success(**payload: Any) -> dict[str, Any]:
    result = {"ok": True, "status": "success"}
    result.update(payload)
    return result


def _error(message: str, **payload: Any) -> dict[str, Any]:
    result = {"ok": False, "status": "error", "message": message}
    result.update(payload)
    return result


SCENE_HANDLERS: dict[str, Callable[[dict[str, Any], dict[str, Any]], dict[str, Any]]] = {
    "spawn_asset": _safe(_handle_spawn_asset),
    "spawn_actor": _safe(_handle_spawn_actor),
    "set_actor_transform": _safe(_handle_set_actor_transform),
    "set_actor_property": _safe(_handle_set_actor_property),
    "get_actor_details": _safe(_handle_get_actor_details),
    "list_actors": _safe(_handle_list_actors),
    "find_actors": _safe(_handle_find_actors),
    "delete_actor": _safe(_handle_delete_actor),
    "duplicate_actor": _safe(_handle_duplicate_actor),
    "drop_actor_to_floor": _safe(_handle_drop_actor_to_floor),
    "get_actor_bounds": _safe(_handle_get_actor_bounds),
    "batch_spawn_assets": _safe(_handle_batch_spawn_assets),
    "align_actors": _safe(_handle_align_actors),
    "set_viewport_camera": _safe(_handle_set_viewport_camera),
    "get_viewport_camera": _safe(_handle_get_viewport_camera),
    "focus_camera_on_actor": _safe(_handle_focus_camera_on_actor),
    "take_screenshot": _safe(_handle_take_screenshot),
    "save_level": _safe(_handle_save_level),
    "new_level": _safe(_handle_new_level),
    "load_level": _safe(_handle_load_level),
}
