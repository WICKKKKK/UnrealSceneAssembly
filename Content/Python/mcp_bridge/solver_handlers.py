"""OBB asset placement solver command handlers."""

from __future__ import annotations

import random
from typing import Any, Callable

import unreal

from .scene_handlers import (
    SceneCommandError,
    _actor_from_params,
    _actor_summary,
    _error,
    _find_actor_by_path,
    _rotator_list,
    _safe,
    _spawn_asset_no_transaction,
    _success,
    _to_rotator,
    _vector_list,
)


_VECTOR_MISSING = object()


def _handle_get_actor_obb(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    obb = _solver_library().get_actor_obb(actor)
    return _success(actor=_actor_summary(actor), obb=_obb_dict(obb))


def _handle_solve_placement(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    scene_obb = _scene_obb_from_params(params)
    candidates = _candidate_structs(params.get("candidates"))
    settings = _settings_struct(params.get("settings") or params)

    results = _solver_library().solve_placement(scene_obb, candidates, settings)
    serialized = [_placement_result_dict(result) for result in results]
    return _success(results=serialized, count=len(serialized))


def _handle_auto_assemble(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    scene_obb = _solver_library().get_actor_obb(actor)
    candidates = _candidate_structs(params.get("candidates"))
    settings = _settings_struct(params.get("settings") or params)
    results = [_placement_result_dict(result) for result in _solver_library().solve_placement(scene_obb, candidates, settings)]
    if not results:
        return _error("Solver returned no placement results.", results=[], count=0)

    chosen_index, random_seed = _pick_result_index(params, len(results))
    spawned = None
    if bool(params.get("auto_place", params.get("spawn", True))):
        best = results[chosen_index]
        label = params.get("label") or params.get("spawn_label")
        spawn_params = {
            "asset_path": best["asset_path"],
            "location": best["transform"]["location"],
            "rotation": best["transform"]["rotation"],
            "scale": best["transform"]["scale"],
            "label": label,
        }
        spawn_tags = params.get("tags")
        result_tag = params.get("result_tag") or params.get("tag")
        if spawn_tags is None and result_tag:
            spawn_tags = [result_tag]
        if spawn_tags:
            spawn_params["tags"] = spawn_tags
        with unreal.ScopedEditorTransaction("Scene Assembly: Auto Assemble Asset"):
            spawned_actor = _spawn_asset_no_transaction(spawn_params)
        spawned = _actor_summary(spawned_actor, include_bounds=True)

    return _success(source_actor=_actor_summary(actor), results=results, count=len(results), spawned=spawned, chosen_index=chosen_index, random_seed=random_seed)


def _handle_auto_assemble_batch(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    values = params.get("items")
    if not isinstance(values, list) or not values:
        raise SceneCommandError("Parameter 'items' must be a non-empty list.")

    auto_place = bool(params.get("auto_place", params.get("spawn", True)))
    default_settings = _settings_struct(params.get("settings") or params)
    items = []
    for index, value in enumerate(values):
        if not isinstance(value, dict):
            items.append({"status": "error", "error": f"Item {index} must be an object.", "results": [], "count": 0})
            continue
        try:
            item_params = dict(params)
            item_params.update(value)
            actor = _actor_from_params(item_params)
            settings = _settings_struct(value.get("settings")) if isinstance(value.get("settings"), dict) else default_settings
            scene_obb = _solver_library().get_actor_obb(actor)
            candidates = _candidate_structs(value.get("candidates"))
            results = [_placement_result_dict(result) for result in _solver_library().solve_placement(scene_obb, candidates, settings)]
            if not results:
                items.append({"actor": _actor_summary(actor), "status": "no_results", "error": "Solver returned no placement results.", "results": [], "count": 0})
                continue

            chosen_index, random_seed = _pick_result_index(item_params, len(results))
            item = {
                "actor": _actor_summary(actor),
                "status": "placed" if auto_place else "solved",
                "results": results,
                "count": len(results),
                "chosen_index": chosen_index,
                "random_seed": random_seed,
                "spawned": None,
            }
            if auto_place:
                best = results[chosen_index]
                spawn_params = {
                    "asset_path": best["asset_path"],
                    "location": best["transform"]["location"],
                    "rotation": best["transform"]["rotation"],
                    "scale": best["transform"]["scale"],
                    "label": value.get("label") or params.get("label") or params.get("spawn_label"),
                }
                spawn_tags = value.get("tags", params.get("tags"))
                result_tag = value.get("result_tag") or value.get("tag") or params.get("result_tag") or params.get("tag")
                if spawn_tags is None and result_tag:
                    spawn_tags = [result_tag]
                if spawn_tags:
                    spawn_params["tags"] = spawn_tags
                item["spawn_params"] = spawn_params
            items.append(item)
        except Exception as exc:
            items.append({"status": "error", "error": str(exc), "results": [], "count": 0})

    spawned_count = 0
    if auto_place:
        with unreal.ScopedEditorTransaction("Scene Assembly: Auto Assemble Batch"):
            for item in items:
                spawn_params = item.pop("spawn_params", None)
                if not spawn_params:
                    continue
                try:
                    spawned_actor = _spawn_asset_no_transaction(spawn_params)
                    item["spawned"] = _actor_summary(spawned_actor, include_bounds=True)
                    spawned_count += 1
                except Exception as exc:
                    item["status"] = "spawn_error"
                    item["error"] = str(exc)

    succeeded = sum(1 for item in items if item.get("status") in {"placed", "solved"})
    return _success(items=items, actor_count=len(items), succeeded=succeeded, spawned_count=spawned_count)


def _handle_get_semantic(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    component = _find_semantic_component(actor)
    return _success(actor=_actor_summary(actor), semantic=_semantic_dict(component))


def _handle_set_semantic(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    actor = _actor_from_params(params)
    category = str(params.get("category") or "")
    description = str(params.get("description") or "")
    tags = _string_list(params.get("tags"))

    with unreal.ScopedEditorTransaction("Scene Assembly: Set Semantic"):
        component = _semantic_component_class().set_actor_semantic(actor, category, description, tags)
    if not component:
        raise SceneCommandError("Failed to add or update SceneSemanticComponent.")

    return _success(actor=_actor_summary(actor), semantic=_semantic_dict(component))


def _handle_solver_self_test(params: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    result = _solver_library().run_solver_self_test()
    if isinstance(result, tuple):
        passed = bool(result[0]) if len(result) > 0 else False
        fit_iou = float(result[1]) if len(result) > 1 else 0.0
        message = str(result[2]) if len(result) > 2 else ""
    else:
        passed = bool(result)
        fit_iou = 0.0
        message = "Solver self-test returned a non-tuple result."
    return _success(passed=passed, fit_iou=fit_iou, message=message)


def _solver_library() -> Any:
    library = getattr(unreal, "SceneAssemblySolverLibrary", None)
    if library is None:
        raise SceneCommandError("SceneAssemblySolverLibrary is unavailable. Rebuild/reload the UnrealSceneAssembly plugin.")
    return library


def _semantic_component_class() -> Any:
    component_class = getattr(unreal, "SceneSemanticComponent", None)
    if component_class is None:
        raise SceneCommandError("SceneSemanticComponent is unavailable. Rebuild/reload the UnrealSceneAssembly plugin.")
    return component_class


def _scene_obb_from_params(params: dict[str, Any]) -> Any:
    actor_path = params.get("actor_path")
    if actor_path:
        return _solver_library().get_actor_obb(_find_actor_by_path(str(actor_path)))

    value = params.get("scene_obb") or params.get("obb")
    if value is None:
        raise SceneCommandError("Parameter 'actor_path' or 'scene_obb' is required.")
    if isinstance(value, getattr(unreal, "SceneOBB", ())):
        return value
    if not isinstance(value, dict):
        raise SceneCommandError("Parameter 'scene_obb' must be an object.")

    obb = unreal.SceneOBB()
    _set_struct_property(obb, "local_center", _vector_from_value(value.get("local_center") or value.get("LocalCenter"), unreal.Vector(0.0, 0.0, 0.0)))
    _set_struct_property(obb, "half_extents", _vector_from_value(value.get("half_extents") or value.get("HalfExtents")))
    _set_struct_property(obb, "world_transform", _transform_from_dict(value.get("world_transform") or value.get("WorldTransform") or {}))
    return obb


def _candidate_structs(values: Any) -> list[Any]:
    if not isinstance(values, list) or not values:
        raise SceneCommandError("Parameter 'candidates' must be a non-empty list.")

    candidates = []
    for index, value in enumerate(values):
        if isinstance(value, getattr(unreal, "AssetCandidate", ())):
            candidates.append(value)
            continue
        if not isinstance(value, dict):
            raise SceneCommandError(f"Candidate {index} must be an object.")

        asset_path = _first_present(value, "asset_path", "AssetPath")
        if not asset_path:
            raise SceneCommandError(f"Candidate {index} is missing asset_path.")

        bbox_center, bbox_half_extents = _candidate_bbox(value, index)
        semantic_score = _first_present(value, "semantic_score", "SemanticScore", "score", default=1.0)
        relative_orientation = _first_present(value, "relative_orientation", "RelativeOrientation", "rel", "relative_pose")
        relative_axes = _first_present(value, "relative_orientation_axes", "RelativeOrientationAxes", "relative_axes")
        num_directions = _first_present(value, "num_directions", "NumDirections", default=1)
        thumbnail_camera = _first_present(value, "thumbnail_camera", "thumbnail_camera_rotation", "ThumbnailCamera", "ThumbnailCameraRotation")

        candidate = unreal.AssetCandidate()
        _set_struct_property(candidate, "asset_path", str(asset_path))
        _set_struct_property(candidate, "bbox_center", bbox_center)
        _set_struct_property(candidate, "bbox_half_extents", bbox_half_extents)
        _set_struct_property(candidate, "semantic_score", float(semantic_score))
        if relative_orientation is not None:
            _set_struct_property(candidate, "b_has_orientation", True)
            _set_struct_property(candidate, "relative_orientation", _to_rotator(relative_orientation))
            _set_struct_property(candidate, "num_directions", int(num_directions or 0))
        if isinstance(relative_axes, dict):
            _set_struct_property(candidate, "b_has_orientation", True)
            _set_struct_property(candidate, "relative_orientation_x", _vector_from_value(_first_present(relative_axes, "x", "X"), unreal.Vector(1.0, 0.0, 0.0)))
            _set_struct_property(candidate, "relative_orientation_y", _vector_from_value(_first_present(relative_axes, "y", "Y"), unreal.Vector(0.0, 1.0, 0.0)))
            _set_struct_property(candidate, "relative_orientation_z", _vector_from_value(_first_present(relative_axes, "z", "Z"), unreal.Vector(0.0, 0.0, 1.0)))
            _set_struct_property(candidate, "num_directions", int(num_directions or 0))
        if thumbnail_camera is not None:
            _set_struct_property(candidate, "b_has_thumbnail_camera", True)
            _set_struct_property(candidate, "thumbnail_camera_rotation", _to_rotator(thumbnail_camera))
        candidates.append(candidate)
    return candidates


def _candidate_bbox(value: dict[str, Any], index: int) -> tuple[Any, Any]:
    bounding_box = _first_present(value, "bounding_box", "BoundingBox")
    bbox_center = _first_present(value, "bbox_center", "BboxCenter", "center")
    bbox_half_extents = _first_present(value, "bbox_half_extents", "BboxHalfExtents", "half_extents")
    bbox_size = _first_present(value, "bbox_size", "BboxSize", "size")

    if isinstance(bounding_box, dict):
        bbox_center = bbox_center if bbox_center is not None else _first_present(bounding_box, "BboxCenter", "bbox_center", "center")
        bbox_half_extents = bbox_half_extents if bbox_half_extents is not None else _first_present(
            bounding_box,
            "BboxHalfExtents",
            "bbox_half_extents",
            "half_extents",
        )
        bbox_size = bbox_size if bbox_size is not None else _first_present(bounding_box, "BboxSize", "bbox_size", "size")

    if bbox_half_extents is None and bbox_size is not None:
        size = _vector_from_value(bbox_size)
        bbox_half_extents = unreal.Vector(size.x * 0.5, size.y * 0.5, size.z * 0.5)
    if bbox_center is None:
        bbox_center = [0.0, 0.0, 0.0]
    if bbox_half_extents is None:
        raise SceneCommandError(f"Candidate {index} is missing bbox_half_extents or bbox_size/bounding_box.BboxSize.")

    return _vector_from_value(bbox_center), _vector_from_value(bbox_half_extents)


def _settings_struct(value: Any) -> Any:
    settings = unreal.SolverSettings()
    if not isinstance(value, dict):
        return settings

    if "scale_mode" in value or "ScaleMode" in value:
        _set_struct_property(settings, "scale_mode", _scale_mode_value(_first_present(value, "scale_mode", "ScaleMode")))
    if "combine_mode" in value or "CombineMode" in value:
        _set_struct_property(settings, "combine_mode", _combine_mode_value(_first_present(value, "combine_mode", "CombineMode")))
    if "orient_mode" in value or "OrientMode" in value:
        _set_struct_property(settings, "orient_mode", _orient_mode_value(_first_present(value, "orient_mode", "OrientMode")))
    if "concept_camera_rotation" in value or "ConceptCameraRotation" in value:
        _set_struct_property(settings, "concept_camera_rotation", _to_rotator(_first_present(value, "concept_camera_rotation", "ConceptCameraRotation")))
    if "orient_basis_rotation" in value or "OrientBasisRotation" in value:
        _set_struct_property(settings, "orient_basis_rotation", _to_rotator(_first_present(value, "orient_basis_rotation", "OrientBasisRotation")))
    if "thumbnail_camera_rotation" in value or "ThumbnailCameraRotation" in value:
        _set_struct_property(settings, "thumbnail_camera_rotation", _to_rotator(_first_present(value, "thumbnail_camera_rotation", "ThumbnailCameraRotation")))

    _set_float_setting(settings, value, "weight_semantic", "WeightSemantic")
    _set_float_setting(settings, value, "weight_geometry", "WeightGeometry")
    _set_float_setting(settings, value, "scale_sensitivity", "ScaleSensitivity")
    _set_float_setting(settings, value, "aspect_sensitivity", "AspectSensitivity")
    _set_bool_setting(settings, value, "normalize_semantic", "bNormalizeSemantic", "b_normalize_semantic")
    _set_int_setting(settings, value, "top_k", "TopK")
    _set_float_setting(settings, value, "final_score_threshold", "FinalScoreThreshold")
    return settings


def _pick_result_index(params: dict[str, Any], result_count: int) -> tuple[int, int | None]:
    """Choose which solved result to place. Top-K of 1 keeps the best result."""
    if result_count <= 1:
        return 0, None
    seed_value = params.get("random_seed")
    if seed_value in (None, "", 0, "0"):
        seed = random.randrange(1, 2147483647)
    else:
        seed = int(seed_value)
    return random.Random(seed).randrange(result_count), seed


def _int_param(params: dict[str, Any], key: str, default: int) -> int:
    value = params.get(key, default)
    return int(default if value in (None, "") else value)


def _set_float_setting(settings: Any, values: dict[str, Any], *names: str) -> None:
    value = _first_present(values, *names)
    if value is not None:
        _set_struct_property(settings, _python_property_name(names[0]), float(value))


def _set_int_setting(settings: Any, values: dict[str, Any], *names: str) -> None:
    value = _first_present(values, *names)
    if value is not None:
        _set_struct_property(settings, _python_property_name(names[0]), int(value))


def _set_bool_setting(settings: Any, values: dict[str, Any], *names: str) -> None:
    value = _first_present(values, *names)
    if value is not None:
        _set_any_struct_property(settings, ("normalize_semantic", "b_normalize_semantic"), bool(value))


def _scale_mode_value(value: Any) -> Any:
    return _enum_value(("SceneAssemblyScaleMode", "ESceneAssemblyScaleMode"), value, ("fit_iou", "fitiou", "fit"))


def _combine_mode_value(value: Any) -> Any:
    return _enum_value(("SceneAssemblyScoreCombineMode", "ESceneAssemblyScoreCombineMode"), value, ("multiplicative", "multiply", "mul"))


def _orient_mode_value(value: Any) -> Any:
    return _enum_value(("SceneAssemblyOrientMode", "ESceneAssemblyOrientMode"), value, ("legacy",))


def _enum_value(type_names: tuple[str, ...], requested: Any, default_names: tuple[str, ...]) -> Any:
    enum_type = None
    for type_name in type_names:
        enum_type = getattr(unreal, type_name, None)
        if enum_type is not None:
            break
    if enum_type is None:
        raise SceneCommandError(f"Enum {type_names[0]} is unavailable.")
    if isinstance(requested, enum_type):
        return requested

    requested_names = {_normalize_enum_name(requested)} if requested is not None else set()
    requested_names.update(default_names if requested is None else ())
    for name in dir(enum_type):
        if name.startswith("_"):
            continue
        value = getattr(enum_type, name)
        normalized = _normalize_enum_name(name)
        if normalized in requested_names:
            return value
    raise SceneCommandError(f"Invalid enum value '{requested}' for {type_names[0]}.")


def _normalize_enum_name(value: Any) -> str:
    return str(value or "").replace(" ", "").replace("-", "").replace("_", "").replace("/", "").lower()


def _find_semantic_component(actor: Any) -> Any:
    component_class = _semantic_component_class()
    try:
        component = actor.get_component_by_class(component_class)
        if component:
            return component
    except Exception:
        pass
    try:
        components = actor.get_components_by_class(component_class)
    except Exception:
        components = []
    return components[0] if components else None


def _semantic_dict(component: Any) -> dict[str, Any]:
    if not component:
        return {"exists": False, "category": "", "description": "", "tags": []}
    return {
        "exists": True,
        "component_path": component.get_path_name() if hasattr(component, "get_path_name") else None,
        "category": str(_get_property(component, "category", "") or ""),
        "description": str(_get_property(component, "description", "") or ""),
        "tags": [str(tag) for tag in (_get_property(component, "tags", []) or [])],
    }


def _obb_dict(obb: Any) -> dict[str, Any]:
    return {
        "local_center": _vector_list(_get_property(obb, "local_center", unreal.Vector(0.0, 0.0, 0.0))),
        "half_extents": _vector_list(_get_property(obb, "half_extents", unreal.Vector(0.0, 0.0, 0.0))),
        "world_transform": _transform_dict(_get_property(obb, "world_transform", unreal.Transform())),
    }


def _placement_result_dict(result: Any) -> dict[str, Any]:
    return {
        "asset_path": str(_get_property(result, "asset_path", "")),
        "transform": _transform_dict(_get_property(result, "transform", unreal.Transform())),
        "fit_iou": float(_get_any_property(result, ("fit_iou", "fit_io_u"), 0.0)),
        "scale_factor": float(_get_property(result, "scale_factor", 1.0)),
        "scale_score": float(_get_property(result, "scale_score", 1.0)),
        "semantic_score": float(_get_property(result, "semantic_score", 1.0)),
        "final_score": float(_get_property(result, "final_score", 0.0)),
        "yaw_step": int(_get_property(result, "yaw_step", 0)),
    }


def _transform_dict(transform: Any) -> dict[str, Any]:
    location = _call_or_property(transform, ("get_translation",), ("translation",), unreal.Vector(0.0, 0.0, 0.0))
    scale = _call_or_property(transform, ("get_scale3d", "get_scale_3d"), ("scale3d", "scale_3d"), unreal.Vector(1.0, 1.0, 1.0))
    rotator = _transform_rotator(transform)
    return {"location": _vector_list(location), "rotation": _rotator_list(rotator), "scale": _vector_list(scale)}


def _transform_from_dict(value: Any) -> Any:
    if isinstance(value, unreal.Transform):
        return value
    if not isinstance(value, dict):
        value = {}
    transform = unreal.Transform()
    _set_struct_property(transform, "translation", _vector_from_value(value.get("location") or value.get("translation"), unreal.Vector(0.0, 0.0, 0.0)))
    _set_struct_property(transform, "scale3d", _vector_from_value(value.get("scale") or value.get("scale3d") or value.get("scale_3d"), unreal.Vector(1.0, 1.0, 1.0)))
    rotation = value.get("rotation")
    if rotation is not None:
        rotator = _to_rotator(rotation)
        _set_struct_property(transform, "rotation", _rotator_to_quat(rotator))
    return transform


def _transform_rotator(transform: Any) -> Any:
    rotator = _call_method(transform, "rotator")
    if rotator is not None:
        return rotator
    rotation = _call_or_property(transform, ("get_rotation",), ("rotation",), None)
    if isinstance(rotation, unreal.Rotator):
        return rotation
    converter = getattr(rotation, "rotator", None)
    if callable(converter):
        return converter()
    return unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)


def _rotator_to_quat(rotator: Any) -> Any:
    converter = getattr(rotator, "quaternion", None)
    if callable(converter):
        return converter()
    converter = getattr(rotator, "to_quat", None)
    if callable(converter):
        return converter()
    return rotator


def _call_or_property(owner: Any, method_names: tuple[str, ...], property_names: tuple[str, ...], default: Any) -> Any:
    for method_name in method_names:
        value = _call_method(owner, method_name)
        if value is not None:
            return value
    for property_name in property_names:
        value = _get_property(owner, property_name, None)
        if value is not None:
            return value
    return default


def _call_method(owner: Any, name: str) -> Any:
    method = getattr(owner, name, None)
    if callable(method):
        try:
            return method()
        except Exception:
            return None
    return None


def _get_property(owner: Any, name: str, default: Any = None) -> Any:
    getter = getattr(owner, "get_editor_property", None)
    if callable(getter):
        try:
            return getter(name)
        except Exception:
            pass
    return getattr(owner, name, default)


def _get_any_property(owner: Any, names: tuple[str, ...], default: Any = None) -> Any:
    for name in names:
        value = _get_property(owner, name, None)
        if value is not None:
            return value
    return default


def _vector_from_value(value: Any, default: Any = _VECTOR_MISSING) -> Any:
    if value is None:
        if default is not _VECTOR_MISSING:
            return default
        raise SceneCommandError("Expected a vector as [x, y, z] or {x, y, z}.")
    if isinstance(value, unreal.Vector):
        return value
    if isinstance(value, dict):
        return unreal.Vector(
            float(_first_present(value, "x", "X", default=0.0)),
            float(_first_present(value, "y", "Y", default=0.0)),
            float(_first_present(value, "z", "Z", default=0.0)),
        )
    if isinstance(value, (list, tuple)) and len(value) == 3:
        return unreal.Vector(float(value[0]), float(value[1]), float(value[2]))
    raise SceneCommandError("Expected a vector as [x, y, z] or {x, y, z}.")


def _set_struct_property(owner: Any, name: str, value: Any) -> None:
    setter = getattr(owner, "set_editor_property", None)
    if callable(setter):
        try:
            setter(name, value)
            return
        except Exception:
            pass
    setattr(owner, name, value)


def _set_any_struct_property(owner: Any, names: tuple[str, ...], value: Any) -> None:
    last_error = None
    for name in names:
        setter = getattr(owner, "set_editor_property", None)
        if callable(setter):
            try:
                setter(name, value)
                return
            except Exception as exc:
                last_error = exc
        if not hasattr(owner, name):
            continue
        try:
            setattr(owner, name, value)
            return
        except Exception as exc:
            last_error = exc
    if last_error:
        raise last_error


def _first_present(values: dict[str, Any], *names: str, default: Any = None) -> Any:
    for name in names:
        if name in values and values[name] is not None:
            return values[name]
    return default


def _python_property_name(name: str) -> str:
    output = []
    for index, char in enumerate(str(name)):
        if char.isupper() and index > 0:
            output.append("_")
        output.append(char.lower())
    return "".join(output)


def _string_list(value: Any) -> list[str]:
    if value is None:
        return []
    if isinstance(value, str):
        return [item.strip() for item in value.split(",") if item.strip()]
    if isinstance(value, (list, tuple)):
        return [str(item) for item in value]
    raise SceneCommandError("Parameter 'tags' must be a string or list of strings.")


SOLVER_HANDLERS: dict[str, Callable[[dict[str, Any], dict[str, Any]], dict[str, Any]]] = {
    "get_actor_obb": _safe(_handle_get_actor_obb),
    "solve_placement": _safe(_handle_solve_placement),
    "auto_assemble": _safe(_handle_auto_assemble),
    "auto_assemble_batch": _safe(_handle_auto_assemble_batch),
    "get_semantic": _safe(_handle_get_semantic),
    "set_semantic": _safe(_handle_set_semantic),
    "solver_self_test": _safe(_handle_solver_self_test),
}
