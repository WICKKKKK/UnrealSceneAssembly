"""In-editor assembly test workflow used by the Slate test panel."""

from __future__ import annotations

import base64
import json
import math
import random
import urllib.request
from typing import Any

import unreal

from clip_retrieval import (
    absolute_public_url,
    candidates_from_hits,
    clip_search_assets_by_image,
    clip_request_json,
    dinov3_search_assets_by_image,
    default_clip_output_fields,
)

from .scene_handlers import (
    SceneCommandError,
    _actor_summary,
    _actor_tags,
    _editor_actor_subsystem,
    _error,
    _find_actor_by_path,
    _spawn_asset_no_transaction,
    _success,
    _handle_focus_camera_on_actor,
)
from .solver_handlers import (
    _candidate_structs as _solver_candidate_structs,
    _pick_result_index,
    _placement_result_dict,
    _settings_struct,
    _solver_library,
)


DEFAULT_RESULT_TAG = "SceneAssemblyResult"
WHITEBOX_TAG = "BlockoutActor"


def get_selected_actor() -> dict[str, Any]:
    actor = _selected_actor()
    return _success(actor=_actor_summary(actor, include_bounds=True))


def get_selection_summary() -> dict[str, Any]:
    return _success(**_selection_summary(_selected_actors()))


def select_all_whiteboxes() -> dict[str, Any]:
    actors = [actor for actor in _editor_actor_subsystem().get_all_level_actors() if actor and _is_blockout(actor)]
    _editor_actor_subsystem().set_selected_level_actors(actors)
    return _success(**_selection_summary(actors))


def deselect_all() -> dict[str, Any]:
    _editor_actor_subsystem().select_nothing()
    return _success(**_selection_summary([]))


def select_actor_by_path(params: dict[str, Any] | None = None) -> dict[str, Any]:
    params = params or {}
    actor = _find_actor_by_path(_required_actor_path(params))
    _editor_actor_subsystem().set_selected_level_actors([actor])
    return _success(actor=_actor_summary(actor, include_bounds=True))


def focus_actor_by_path(params: dict[str, Any] | None = None) -> dict[str, Any]:
    params = params or {}
    actor = _find_actor_by_path(_required_actor_path(params))
    _editor_actor_subsystem().set_selected_level_actors([actor])
    return _handle_focus_camera_on_actor({"actor_path": actor.get_path_name()}, {})


def cleanup_assembly_results(params: dict[str, Any] | None = None) -> dict[str, Any]:
    params = params or {}
    tag = _result_tag(params)
    deleted = _cleanup_tagged(tag)
    return _success(tag=tag, deleted_count=deleted)


def run_assembly_test(params: dict[str, Any] | None = None) -> dict[str, Any]:
    params = params or {}
    tag = _result_tag(params)
    capture_context = _capture_context(params)
    actors = capture_context["actors"]
    skipped_non_whitebox: list[dict[str, Any]] = []
    if not actors:
        return _error(
            "No processable actors in the capture JSON. Capture selected Blockout actors first.",
            items=[],
            actor_count=0,
            skipped_non_whitebox=skipped_non_whitebox,
            cleanup={"tag": tag, "deleted_count": 0},
        )

    base_seed = _base_random_seed(params)
    items = [
        _solve_one(actor, capture_context["entry_by_path"].get(actor.get_path_name(), {}), capture_context, params, tag, base_seed, index)
        for index, actor in enumerate(actors)
    ]

    deleted = 0
    spawned_count = 0
    with unreal.ScopedEditorTransaction("Scene Assembly: Batch Solve and Place"):
        deleted = _cleanup_tagged_no_transaction(tag)
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

    succeeded = sum(1 for item in items if item.get("status") == "placed")
    return _success(
        items=items,
        actor_count=len(actors),
        succeeded=succeeded,
        spawned_count=spawned_count,
        skipped_non_whitebox=skipped_non_whitebox,
        cleanup={"tag": tag, "deleted_count": deleted},
        random_seed=base_seed,
        whitebox_only=_bool_param(params, "whitebox_only", True),
        capture_json_path=capture_context["capture_json_path"],
        concept_art_path=capture_context["concept_art_path"],
    )


def _solve_one(actor: Any, id_entry: dict[str, Any], capture_context: dict[str, Any], params: dict[str, Any], tag: str, base_seed: int | None, actor_index: int) -> dict[str, Any]:
    item: dict[str, Any] = {
        "actor": _actor_summary(actor, include_bounds=True),
        "status": "pending",
        "results": [],
        "count": 0,
        "chosen_index": 0,
        "random_seed": None,
        "spawned": None,
    }

    try:
        bbox_source = _crop_bbox_source(params)
        bbox = _bbox_from_entry(id_entry, bbox_source)
        item["crop_bbox_source"] = bbox_source
        item["pixel_bbox"] = _bbox_from_entry(id_entry, "pixel_bbox")
        item["full_bbox"] = _bbox_from_entry(id_entry, "full_bbox")
        if not bbox:
            item["status"] = "no_full_bbox" if bbox_source == "full_bbox" else "no_pixel_bbox"
            item["error"] = f"Capture JSON does not contain a valid {bbox_source} for this actor."
            return item

        data_uri = _call_unreal_static(
            _scene_capture_library(),
            "crop_image_region_to_base64",
            capture_context["concept_art_path"],
            int(capture_context["image_width"]),
            int(capture_context["image_height"]),
            int(bbox[0]),
            int(bbox[1]),
            int(bbox[2]),
            int(bbox[3]),
            max(0, _int_param(params, "crop_expand_pixels", 20)),
        )
        if not data_uri:
            item["status"] = "crop_error"
            item["error"] = "Failed to crop the concept art image for this actor."
            return item

        candidate_limit = _int_param(params, "candidate_limit", 20)
        timeout = _float_param(params, "timeout", 15.0)
        retrieval_model = _retrieval_model(params)
        common_search_kwargs = {
            "image_url": data_uri,
            "limit": candidate_limit,
            "project_names": _optional_string_list(params.get("project_names")),
            "asset_types": _optional_string_list(params.get("asset_types")),
            "filters": params.get("filters") if isinstance(params.get("filters"), dict) else None,
            "output_fields": default_clip_output_fields(include_bounding_box=True),
            "ef": _int_param(params, "ef", 64),
            "timeout": timeout,
        }
        if retrieval_model == "dinov3":
            search = dinov3_search_assets_by_image(
                **common_search_kwargs,
                collection=_optional_string(params.get("collection_dinov3") or params.get("dinov3_collection")),
            )
        else:
            search = clip_search_assets_by_image(
                **common_search_kwargs,
                score_threshold=_float_param(params, "score_threshold", 0.0),
                collection=_optional_string(params.get("collection") or params.get("collection_clip") or params.get("clip_collection")),
            )
        item["retrieval_model"] = retrieval_model
        candidates, skipped = candidates_from_hits(search.get("hits") or [])
        item["search"] = _search_summary(search, candidates, skipped)
        if not candidates:
            item["status"] = "no_candidates"
            item["error"] = "Image search returned no candidates with asset_path and bounding_box."
            return item

        orient_mode = _orient_mode(params)
        item["orient_mode"] = orient_mode
        candidates = _with_orientation(candidates, data_uri, orient_mode, timeout)

        settings_payload = params.get("settings") or _settings_from_params(params)
        settings_payload = _settings_with_camera(settings_payload, capture_context, orient_mode)
        settings = _settings_struct(settings_payload)
        scene_obb = _solver_library().get_actor_obb(actor)
        results = [_placement_result_dict(result) for result in _solver_library().solve_placement(scene_obb, _candidate_structs(candidates), settings)]
        item["results"] = results
        item["count"] = len(results)
        if not results:
            item["status"] = "no_results"
            item["error"] = "Solver returned no placement results."
            return item

        pick_params = dict(params)
        if base_seed is not None and len(results) > 1:
            pick_params["random_seed"] = base_seed + actor_index
        chosen_index, random_seed = _pick_result_index(pick_params, len(results))
        best = results[chosen_index]
        label = _optional_string(params.get("label")) or f"SceneAssembly_{_safe_label(actor.get_actor_label())}"
        item["chosen_index"] = chosen_index
        item["random_seed"] = random_seed
        item["spawn_params"] = {
            "asset_path": best["asset_path"],
            "location": best["transform"]["location"],
            "rotation": best["transform"]["rotation"],
            "scale": best["transform"]["scale"],
            "label": label,
            "tags": [tag],
        }
        item["status"] = "placed"
        return item
    except Exception as exc:
        item["status"] = "error"
        item["error"] = str(exc)
        return item


def _selected_actor() -> Any:
    actors = _selected_actors()
    if len(actors) != 1:
        raise SceneCommandError(f"Select exactly one actor. Current selection count: {len(actors)}")
    return actors[0]


def _selected_actors() -> list[Any]:
    return [actor for actor in _editor_actor_subsystem().get_selected_level_actors() if actor]


def _resolve_actors(params: dict[str, Any]) -> tuple[list[Any], list[dict[str, Any]]]:
    actor_paths = params.get("actor_paths")
    if isinstance(actor_paths, str):
        actor_paths = [path.strip() for path in actor_paths.split(",") if path.strip()]
    if isinstance(actor_paths, (list, tuple)) and actor_paths:
        actors = []
        for path in actor_paths:
            try:
                actors.append(_find_actor_by_path(str(path)))
            except Exception:
                continue
    else:
        actors = _selected_actors()

    skipped_non_whitebox: list[dict[str, Any]] = []
    if _bool_param(params, "whitebox_only", True):
        filtered = []
        for actor in actors:
            if _is_blockout(actor):
                filtered.append(actor)
            else:
                skipped_non_whitebox.append(_selection_actor_summary(actor))
        actors = filtered
    return actors, skipped_non_whitebox


def _capture_context(params: dict[str, Any]) -> dict[str, Any]:
    capture_json_path = _optional_string(params.get("capture_json_path"))
    concept_art_path = _optional_string(params.get("concept_art_path"))
    if not capture_json_path:
        raise SceneCommandError("Parameter 'capture_json_path' is required. Capture the aesthetic reference first.")
    if not concept_art_path:
        raise SceneCommandError("Parameter 'concept_art_path' is required. Upload a concept art image first.")

    with open(capture_json_path, "r", encoding="utf-8") as handle:
        capture_data = json.load(handle)
    if not isinstance(capture_data, dict):
        raise SceneCommandError("Capture JSON is invalid.")

    image_width, image_height = _capture_image_size(capture_data)
    entries = capture_data.get("id_map")
    if not isinstance(entries, list):
        raise SceneCommandError("Capture JSON does not contain an id_map array.")

    actors: list[Any] = []
    entry_by_path: dict[str, dict[str, Any]] = {}
    missing_actor_paths: list[str] = []
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        actor_path = _optional_string(entry.get("actor_path"))
        if not actor_path:
            continue
        try:
            actor = _find_actor_by_path(actor_path)
        except Exception:
            missing_actor_paths.append(actor_path)
            continue
        actors.append(actor)
        entry_by_path[actor_path] = entry

    return {
        "capture_json_path": capture_json_path,
        "concept_art_path": concept_art_path,
        "capture_data": capture_data,
        "entries": entries,
        "actors": actors,
        "entry_by_path": entry_by_path,
        "missing_actor_paths": missing_actor_paths,
        "image_width": image_width,
        "image_height": image_height,
        "params": dict(params),
    }


def _call_unreal_static(library: Any, snake_name: str, *args: Any) -> Any:
    method = getattr(library, snake_name, None)
    if method is None:
        parts = snake_name.split("_")
        method = getattr(library, parts[0] + "".join(part[:1].upper() + part[1:] for part in parts[1:]), None)
    if method is None:
        raise SceneCommandError(f"{library} does not expose {snake_name}.")
    result = method(*args)
    if isinstance(result, tuple):
        return next((value for value in result if isinstance(value, str)), result[0] if result else None)
    return result


def _capture_image_size(capture_data: dict[str, Any]) -> tuple[int, int]:
    image_size = capture_data.get("image_size")
    if isinstance(image_size, dict):
        width = int(image_size.get("width") or 0)
        height = int(image_size.get("height") or 0)
        if width > 0 and height > 0:
            return width, height
    camera = capture_data.get("camera")
    if isinstance(camera, dict):
        resolution = camera.get("resolution")
        if isinstance(resolution, dict):
            width = int(resolution.get("width") or 0)
            height = int(resolution.get("height") or 0)
            if width > 0 and height > 0:
                return width, height
    raise SceneCommandError("Capture JSON does not contain a valid image_size/resolution.")


def _pixel_bbox(entry: dict[str, Any]) -> list[int] | None:
    return _bbox_from_entry(entry, "pixel_bbox")


def _bbox_from_entry(entry: dict[str, Any], field_name: str) -> list[int] | None:
    bbox = entry.get(field_name) if isinstance(entry, dict) else None
    if not isinstance(bbox, list) or len(bbox) != 4:
        return None
    values = [int(value) for value in bbox]
    if values[0] > values[2] or values[1] > values[3]:
        return None
    return values


def _crop_bbox_source(params: dict[str, Any]) -> str:
    value = _optional_string(params.get("crop_bbox_source")) or "full_bbox"
    normalized = value.strip().lower().replace("-", "_").replace(" ", "_")
    if normalized in {"pixel", "pixels", "visible", "visible_pixels", "pixel_bbox"}:
        return "pixel_bbox"
    return "full_bbox"


def _scene_capture_library() -> Any:
    library = getattr(unreal, "SceneCaptureLibrary", None)
    if library is None:
        raise SceneCommandError("SceneCaptureLibrary is unavailable. Rebuild/reload the SceneCapture plugin.")
    return library


def _is_whitebox(actor: Any) -> bool:
    return WHITEBOX_TAG in _actor_tags(actor)


def _is_blockout(actor: Any) -> bool:
    if not actor:
        return False
    try:
        blockout_class = unreal.load_class(None, "/Script/Blockout.BlockoutBaseDynamicMeshActor")
        actor_class = actor.get_class() if hasattr(actor, "get_class") else None
        current_class = actor_class
        while blockout_class and current_class:
            if current_class == blockout_class:
                return True
            current_class = current_class.get_super_class() if hasattr(current_class, "get_super_class") else None
        if actor_class and str(actor_class.get_path_name()).startswith("/Script/Blockout."):
            return True
    except Exception:
        pass
    return _is_whitebox(actor)


def _selection_summary(actors: list[Any]) -> dict[str, Any]:
    actor_items = [_selection_actor_summary(actor) for actor in actors]
    return {
        "selected_count": len(actor_items),
        "whitebox_count": sum(1 for item in actor_items if item.get("is_whitebox")),
        "actors": actor_items,
    }


def _selection_actor_summary(actor: Any) -> dict[str, Any]:
    tags = _actor_tags(actor)
    return {
        "label": actor.get_actor_label() if hasattr(actor, "get_actor_label") else str(actor),
        "actor_path": actor.get_path_name() if hasattr(actor, "get_path_name") else "",
        "is_whitebox": _is_blockout(actor),
        "tags": tags,
    }


def _cleanup_tagged(tag: str) -> int:
    with unreal.ScopedEditorTransaction("Scene Assembly: Cleanup Test Results"):
        return _cleanup_tagged_no_transaction(tag)


def _cleanup_tagged_no_transaction(tag: str) -> int:
    if not tag:
        raise SceneCommandError("Result tag must not be empty.")
    actors = [actor for actor in _editor_actor_subsystem().get_all_level_actors() if actor and tag in _actor_tags(actor)]
    deleted = 0
    for actor in actors:
        if _editor_actor_subsystem().destroy_actor(actor):
            deleted += 1
    return deleted


def _candidate_structs(candidates: list[dict[str, Any]]) -> list[Any]:
    return _solver_candidate_structs(candidates)


def _search_summary(search: dict[str, Any], candidates: list[dict[str, Any]], skipped: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "collection_name": search.get("collection_name"),
        "model": search.get("model"),
        "search_time_ms": search.get("search_time_ms"),
        "hit_count": search.get("count", 0),
        "candidate_count": len(candidates),
        "skipped": skipped,
        "filters": search.get("filters"),
    }


def _settings_from_params(params: dict[str, Any]) -> dict[str, Any]:
    keys = (
        "scale_mode",
        "combine_mode",
        "orient_mode",
        "concept_camera_rotation",
        "orient_basis_rotation",
        "thumbnail_camera_rotation",
        "weight_semantic",
        "weight_geometry",
        "scale_sensitivity",
        "aspect_sensitivity",
        "normalize_semantic",
        "top_k",
        "final_score_threshold",
    )
    return {key: params[key] for key in keys if key in params and params[key] is not None}


def _result_tag(params: dict[str, Any]) -> str:
    return _optional_string(params.get("result_tag") or params.get("tag")) or DEFAULT_RESULT_TAG


def _required_actor_path(params: dict[str, Any]) -> str:
    actor_path = _optional_string(params.get("actor_path"))
    if not actor_path:
        raise SceneCommandError("Parameter 'actor_path' is required.")
    return actor_path


def _retrieval_model(params: dict[str, Any]) -> str:
    value = str(params.get("retrieval_model") or "DINOv3").strip().lower()
    if value in {"dinov3", "dino", "dino_v3"}:
        return "dinov3"
    return "clip"


def _orient_mode(params: dict[str, Any]) -> str:
    value = str(params.get("orient_mode") or "Precomputed").strip().lower().replace("-", "_").replace(" ", "_")
    if value in {"legacy", "off", "none"}:
        return "Legacy"
    if value in {"dual", "dual_image", "dualimage", "two_image", "twoimage"}:
        return "DualImage"
    return "Precomputed"


def _settings_with_camera(settings: dict[str, Any], capture_context: dict[str, Any], orient_mode: str) -> dict[str, Any]:
    output = dict(settings or {})
    output["orient_mode"] = orient_mode
    camera_rotation = _capture_camera_rotation(capture_context)
    if camera_rotation is not None:
        output["concept_camera_rotation"] = camera_rotation
    raw_params = capture_context.get("params")
    params = raw_params if isinstance(raw_params, dict) else {}
    for key in ("orient_basis_rotation", "thumbnail_camera_rotation"):
        if key in params and key not in output:
            output[key] = params[key]
    return output


def _capture_camera_rotation(capture_context: dict[str, Any]) -> Any:
    capture_data = capture_context.get("capture_data")
    camera = capture_data.get("camera") if isinstance(capture_data, dict) else None
    rotation = camera.get("rotation") if isinstance(camera, dict) else None
    return rotation if isinstance(rotation, dict) else None


def _with_orientation(candidates: list[dict[str, Any]], data_uri: str, orient_mode: str, timeout: float) -> list[dict[str, Any]]:
    if orient_mode == "Legacy":
        return candidates
    if orient_mode == "DualImage":
        return _with_dual_image_orientation(candidates, data_uri, timeout)
    return _with_precomputed_orientation(candidates, data_uri, timeout)


def _with_precomputed_orientation(candidates: list[dict[str, Any]], data_uri: str, timeout: float) -> list[dict[str, Any]]:
    response = _orient_predict(data_uri, do_rm_bkg_ref=True, do_rm_bkg_tgt=True, timeout=timeout)
    crop_pose = response.get("ref") if isinstance(response, dict) else None
    if not isinstance(crop_pose, dict):
        return candidates
    output = []
    for candidate in candidates:
        item = dict(candidate)
        thumb_pose = candidate.get("orient_pose")
        if isinstance(thumb_pose, dict):
            rel = _relative_pose_from_absolute(crop_pose, thumb_pose)
            item["relative_orientation"] = rel
            item["relative_orientation_axes"] = _pose_axes(rel)
            item["num_directions"] = int(thumb_pose.get("num_directions", 1) or 0)
            if isinstance(candidate.get("thumbnail_camera"), dict):
                item["thumbnail_camera"] = candidate.get("thumbnail_camera")
        output.append(item)
    return output


def _candidate_orientation_image_ref(candidate: dict[str, Any]) -> str:
    return str(
        candidate.get("orient_thumbnail_abs_url")
        or absolute_public_url(str(candidate.get("orient_thumbnail_url") or candidate.get("thumbnail_url") or ""))
        or ""
    ).strip()


def _http_image_to_data_uri(image_ref: str, timeout: float) -> str:
    value = str(image_ref or "").strip()
    if not value:
        return ""
    if value.lower().startswith("data:"):
        return value
    url = absolute_public_url(value)
    request = urllib.request.Request(url, headers={"Accept": "image/*"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        image_bytes = response.read()
        content_type = response.headers.get_content_type() if hasattr(response.headers, "get_content_type") else None
    if not image_bytes:
        raise RuntimeError("empty image response: {0}".format(url))
    if not content_type:
        content_type = "image/png"
    return "data:{0};base64,{1}".format(content_type, base64.b64encode(image_bytes).decode("ascii"))


def _with_dual_image_orientation(candidates: list[dict[str, Any]], data_uri: str, timeout: float) -> list[dict[str, Any]]:
    image_refs = [_candidate_orientation_image_ref(candidate) for candidate in candidates]
    if any(not image_ref for image_ref in image_refs):
        return candidates
    try:
        image_tgts = [_http_image_to_data_uri(image_ref, timeout) for image_ref in image_refs]
    except Exception as exc:
        unreal.log_warning("[SceneAssembly] DualImage orient target download failed: {0}".format(exc))
        return candidates
    payload = {
        "image_ref": data_uri,
        "image_tgts": image_tgts,
        "do_rm_bkg_ref": True,
        "do_rm_bkg_tgt": False,
    }
    response = clip_request_json("/orient/predict/shared_ref", payload, timeout=timeout)
    results = response.get("results") if isinstance(response, dict) else None
    if not isinstance(results, list):
        return candidates
    if len(results) != len(candidates):
        return candidates
    output = []
    for candidate, result in zip(candidates, results):
        item = dict(candidate)
        rel = result.get("rel") if isinstance(result, dict) else None
        ref = result.get("ref") if isinstance(result, dict) else None
        if isinstance(rel, dict):
            item["relative_orientation"] = rel
            item["relative_orientation_axes"] = _pose_axes(rel)
            if isinstance(ref, dict):
                item["num_directions"] = int(ref.get("num_directions", candidate.get("num_directions", 1)) or 0)
            if isinstance(candidate.get("thumbnail_camera"), dict):
                item["thumbnail_camera"] = candidate.get("thumbnail_camera")
        output.append(item)
    return output


def _orient_predict(image_ref: str, do_rm_bkg_ref: bool, do_rm_bkg_tgt: bool, timeout: float) -> dict[str, Any]:
    return clip_request_json(
        "/orient/predict",
        {
            "image_ref": image_ref,
            "do_rm_bkg_ref": bool(do_rm_bkg_ref),
            "do_rm_bkg_tgt": bool(do_rm_bkg_tgt),
        },
        timeout=timeout,
    )


def _orient_preprocess(image_ref: str, timeout: float) -> dict[str, Any]:
    return clip_request_json(
        "/orient/preprocess",
        {"image": image_ref},
        timeout=timeout,
    )


def _relative_pose_from_absolute(crop_pose: dict[str, Any], thumb_pose: dict[str, Any]) -> dict[str, float]:
    crop_matrix = _pose_matrix(crop_pose)
    thumb_matrix = _pose_matrix(thumb_pose)
    rel = _mat_mul(crop_matrix, _mat_transpose(thumb_matrix))
    return _matrix_to_pose(rel)


def _pose_axes(pose: dict[str, Any]) -> dict[str, list[float]]:
    matrix = _pose_matrix(pose)
    return {
        "x": [matrix[0][0], matrix[1][0], matrix[2][0]],
        "y": [matrix[0][1], matrix[1][1], matrix[2][1]],
        "z": [matrix[0][2], matrix[1][2], matrix[2][2]],
    }


def _pose_matrix(pose: dict[str, Any]) -> list[list[float]]:
    az = math.radians(float(pose.get("azimuth", 0.0)))
    el = math.radians(float(pose.get("polar", pose.get("elevation", 0.0))))
    ro = math.radians(float(pose.get("rotation", 0.0)))
    return _mat_mul(_rot_x(ro), _mat_mul(_rot_y(el), _rot_z(-az)))


def _matrix_to_pose(matrix: list[list[float]]) -> dict[str, float]:
    # Inverse of R = Rx(rot) * Ry(polar) * Rz(-azimuth), matching Orient-Anything utils.
    sin_el = max(-1.0, min(1.0, matrix[0][2]))
    el = math.asin(sin_el)
    cos_el = math.cos(el)
    if abs(cos_el) > 1.0e-6:
        ro = math.atan2(-matrix[1][2], matrix[2][2])
        az = math.atan2(-matrix[0][1], matrix[0][0])
    else:
        ro = 0.0
        az = math.atan2(matrix[1][0], matrix[1][1])
    return {
        "azimuth": math.degrees(az),
        "polar": math.degrees(el),
        "rotation": math.degrees(ro),
    }


def _rot_x(angle: float) -> list[list[float]]:
    c = math.cos(angle)
    s = math.sin(angle)
    return [[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]]


def _rot_y(angle: float) -> list[list[float]]:
    c = math.cos(angle)
    s = math.sin(angle)
    return [[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]]


def _rot_z(angle: float) -> list[list[float]]:
    c = math.cos(angle)
    s = math.sin(angle)
    return [[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]]


def _mat_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [[sum(a[row][k] * b[k][col] for k in range(3)) for col in range(3)] for row in range(3)]


def _mat_transpose(matrix: list[list[float]]) -> list[list[float]]:
    return [[matrix[col][row] for col in range(3)] for row in range(3)]


def _base_random_seed(params: dict[str, Any]) -> int | None:
    if _int_param(params, "top_k", 1) <= 1:
        return None
    seed_value = params.get("random_seed")
    if seed_value in (None, "", 0, "0"):
        return random.randrange(1, 2147483647)
    return int(seed_value)


def _optional_string(value: Any) -> str | None:
    text = str(value or "").strip()
    return text or None


def _optional_string_list(value: Any) -> list[str] | None:
    if value is None:
        return None
    if isinstance(value, str):
        items = [item.strip() for item in value.split(",") if item.strip()]
        return items or None
    if isinstance(value, (list, tuple)):
        items = [str(item).strip() for item in value if str(item).strip()]
        return items or None
    return None


def _bool_param(params: dict[str, Any], key: str, default: bool) -> bool:
    value = params.get(key, default)
    if isinstance(value, str):
        return value.strip().lower() not in {"0", "false", "no", "off", ""}
    return bool(value)


def _int_param(params: dict[str, Any], key: str, default: int) -> int:
    value = params.get(key, default)
    return int(default if value in (None, "") else value)


def _float_param(params: dict[str, Any], key: str, default: float) -> float:
    value = params.get(key, default)
    return float(default if value in (None, "") else value)


def _safe_label(value: Any) -> str:
    text = str(value or "Actor").strip() or "Actor"
    return "".join(char if char.isalnum() or char in "_-" else "_" for char in text)
