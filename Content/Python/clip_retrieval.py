"""Shared CLIP retrieval helpers for UnrealSceneAssembly."""

from __future__ import annotations

import json
import socket
from typing import Any
import urllib.error
import urllib.request


def clip_config() -> dict[str, Any]:
    try:
        import config as scene_config
    except Exception as exc:
        raise RuntimeError(f"Failed to import Content/Python/config.py: {exc}") from exc

    api_key = str(getattr(scene_config, "API_KEY", "") or "").strip()
    if not api_key:
        raise RuntimeError("API_KEY is not configured in Content/Python/config.py")

    return {
        "base_url": str(getattr(scene_config, "BASE_URL", "http://localhost:8000") or "").rstrip("/"),
        "api_prefix": str(getattr(scene_config, "API_PREFIX", "/api/v1") or "").rstrip("/"),
        "api_key": api_key,
        "collection": str(getattr(scene_config, "COLLECTION_CLIP", "clip_assets_test") or "clip_assets_test"),
        "collection_clip": str(getattr(scene_config, "COLLECTION_CLIP", "clip_assets_test") or "clip_assets_test"),
        "collection_dinov3": str(getattr(scene_config, "COLLECTION_DINOv3", "dinov3_assets_test") or "dinov3_assets_test"),
        "project_name": str(getattr(scene_config, "PROJECT_NAME", "") or ""),
        "timeout": float(getattr(scene_config, "HTTP_TIMEOUT_SECONDS", 120.0) or 120.0),
    }


def clip_api_url(path: str, cfg: dict[str, Any]) -> str:
    path = path if path.startswith("/") else f"/{path}"
    return f"{cfg['base_url']}{cfg['api_prefix']}{path}"


def clip_request_json(path: str, payload: dict[str, Any], timeout: float | None = None) -> dict[str, Any]:
    cfg = clip_config()
    body = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        clip_api_url(path, cfg),
        data=body,
        method="POST",
        headers={
            "Authorization": f"Bearer {cfg['api_key']}",
            "Content-Type": "application/json",
            "Accept": "application/json",
        },
    )

    try:
        with urllib.request.urlopen(request, timeout=timeout or cfg["timeout"]) as response:
            raw = response.read().decode("utf-8")
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")[:1000]
        raise RuntimeError(f"JadeServices HTTP {exc.code}: {detail or exc.reason}") from exc
    except (urllib.error.URLError, socket.timeout, TimeoutError) as exc:
        raise RuntimeError(f"Failed to reach JadeServices: {exc}") from exc

    try:
        envelope = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"JadeServices returned invalid JSON: {exc}") from exc

    code = envelope.get("code")
    if code not in (None, 0, 200):
        message = envelope.get("message") or "JadeServices returned an error"
        raise RuntimeError(f"JadeServices code {code}: {message}")

    data = envelope.get("data")
    if not isinstance(data, dict):
        raise RuntimeError("JadeServices response did not include a data object")
    return data


def clip_error(exc: Exception) -> dict[str, Any]:
    return {
        "ok": False,
        "status": "error",
        "message": str(exc),
    }


def default_clip_output_fields(include_bounding_box: bool = False) -> list[str]:
    fields = ["thumbnail_url", "asset_name", "asset_path", "asset_type", "public_path"]
    if include_bounding_box:
        fields.append("bounding_box")
    return fields


def clip_search_assets(
    text: str,
    limit: int = 10,
    offset: int = 0,
    project_names: list[str] | None = None,
    asset_types: list[str] | None = None,
    filters: dict[str, Any] | None = None,
    output_fields: list[str] | None = None,
    score_threshold: float = 0.0,
    ef: int = 64,
    collection: str | None = None,
    timeout: float | None = None,
) -> dict[str, Any]:
    query = str(text or "").strip()
    if not query:
        raise ValueError("text must not be empty")

    cfg = clip_config()
    collection_name = (collection or cfg["collection"]).strip()
    if not collection_name:
        raise ValueError("collection must not be empty")

    request_filters = build_clip_filters(project_names=project_names, asset_types=asset_types, filters=filters)
    payload: dict[str, Any] = {
        "text": query,
        "limit": bounded_int(limit, 1, 100),
        "offset": max(0, int(offset)),
        "search_params": {
            "mode": "hnsw",
            "ef": bounded_int(ef, 1, 512),
            "score_threshold": float(score_threshold),
        },
    }
    if request_filters:
        payload["filters"] = request_filters
    if output_fields is not None:
        payload["output_fields"] = output_fields

    data = clip_request_json(f"/clip/collections/{collection_name}/search/single_text", payload, timeout or cfg["timeout"])
    hits = data.get("hits") or []
    return {
        "ok": True,
        "status": "success",
        "collection_name": data.get("collection_name", collection_name),
        "model": data.get("model"),
        "query_vector_dim": data.get("query_vector_dim"),
        "search_time_ms": data.get("search_time_ms"),
        "count": len(hits),
        "hits": hits,
        "filters": request_filters,
    }


def clip_search_assets_by_image(
    image_url: str,
    limit: int = 10,
    offset: int = 0,
    project_names: list[str] | None = None,
    asset_types: list[str] | None = None,
    filters: dict[str, Any] | None = None,
    output_fields: list[str] | None = None,
    score_threshold: float = 0.0,
    ef: int = 64,
    collection: str | None = None,
    timeout: float | None = None,
) -> dict[str, Any]:
    query_image = str(image_url or "").strip()
    if not query_image:
        raise ValueError("image_url must not be empty")

    cfg = clip_config()
    collection_name = (collection or cfg["collection"]).strip()
    if not collection_name:
        raise ValueError("collection must not be empty")

    request_filters = build_clip_filters(project_names=project_names, asset_types=asset_types, filters=filters)
    payload: dict[str, Any] = {
        "image": {"url": query_image},
        "limit": bounded_int(limit, 1, 100),
        "offset": max(0, int(offset)),
        "search_params": {
            "mode": "hnsw",
            "ef": bounded_int(ef, 1, 512),
            "score_threshold": float(score_threshold),
        },
    }
    if request_filters:
        payload["filters"] = request_filters
    if output_fields is not None:
        payload["output_fields"] = output_fields

    data = clip_request_json(f"/clip/collections/{collection_name}/search/single_image", payload, timeout or cfg["timeout"])
    hits = data.get("hits") or []
    return {
        "ok": True,
        "status": "success",
        "collection_name": data.get("collection_name", collection_name),
        "model": data.get("model"),
        "query_vector_dim": data.get("query_vector_dim"),
        "search_time_ms": data.get("search_time_ms"),
        "count": len(hits),
        "hits": hits,
        "filters": request_filters,
    }


def dinov3_search_assets_by_image(
    image_url: str,
    limit: int = 10,
    offset: int = 0,
    project_names: list[str] | None = None,
    asset_types: list[str] | None = None,
    filters: dict[str, Any] | None = None,
    output_fields: list[str] | None = None,
    ef: int = 64,
    collection: str | None = None,
    timeout: float | None = None,
) -> dict[str, Any]:
    query_image = str(image_url or "").strip()
    if not query_image:
        raise ValueError("image_url must not be empty")

    cfg = clip_config()
    collection_name = (collection or cfg["collection_dinov3"]).strip()
    if not collection_name:
        raise ValueError("collection must not be empty")

    request_filters = build_clip_filters(project_names=project_names, asset_types=asset_types, filters=filters)
    payload: dict[str, Any] = {
        "image": {"base64": image_data_uri_to_base64(query_image)},
        "limit": bounded_int(limit, 1, 100),
        "offset": max(0, int(offset)),
        "search_params": {
            "ef": bounded_int(ef, 1, 512),
        },
    }
    if request_filters:
        payload["filters"] = request_filters
    if output_fields is not None:
        payload["output_fields"] = output_fields

    data = clip_request_json(f"/dinov3/collections/{collection_name}/search/global", payload, timeout or cfg["timeout"])
    hits = data.get("hits") or []
    return {
        "ok": True,
        "status": "success",
        "collection_name": data.get("collection_name", collection_name),
        "model": data.get("model"),
        "query_vector_dim": data.get("query_vector_dim"),
        "search_time_ms": data.get("search_time_ms"),
        "count": len(hits),
        "hits": hits,
        "filters": request_filters,
    }


def image_data_uri_to_base64(image_url: str) -> str:
    value = str(image_url or "").strip()
    if value.lower().startswith("data:image/"):
        _, sep, encoded = value.partition(",")
        return encoded.strip() if sep else value
    return value


def solver_settings_payload(
    scale_mode: str | None = None,
    combine_mode: str | None = None,
    weight_semantic: float | None = None,
    weight_geometry: float | None = None,
    scale_sensitivity: float | None = None,
    aspect_sensitivity: float | None = None,
    normalize_semantic: bool | None = None,
    top_k: int | None = None,
    final_score_threshold: float | None = None,
) -> dict[str, Any]:
    settings: dict[str, Any] = {}
    if scale_mode is not None:
        settings["scale_mode"] = scale_mode
    if combine_mode is not None:
        settings["combine_mode"] = combine_mode
    if weight_semantic is not None:
        settings["weight_semantic"] = weight_semantic
    if weight_geometry is not None:
        settings["weight_geometry"] = weight_geometry
    if scale_sensitivity is not None:
        settings["scale_sensitivity"] = scale_sensitivity
    if aspect_sensitivity is not None:
        settings["aspect_sensitivity"] = aspect_sensitivity
    if normalize_semantic is not None:
        settings["normalize_semantic"] = normalize_semantic
    if top_k is not None:
        settings["top_k"] = top_k
    if final_score_threshold is not None:
        settings["final_score_threshold"] = final_score_threshold
    return settings


def hit_field(hit: dict[str, Any], name: str, default: Any = None) -> Any:
    if name in hit and hit[name] is not None:
        return hit[name]
    fields = hit.get("fields")
    if isinstance(fields, dict) and fields.get(name) is not None:
        return fields.get(name)
    entity = hit.get("entity")
    if isinstance(entity, dict) and entity.get(name) is not None:
        return entity.get(name)
    return default


def candidate_from_hit(hit: Any) -> dict[str, Any] | None:
    if not isinstance(hit, dict):
        return None
    asset_path = hit_field(hit, "asset_path")
    bounding_box = hit_field(hit, "bounding_box")
    if not asset_path or not isinstance(bounding_box, dict):
        return None
    score = hit_field(hit, "score", hit_field(hit, "distance", 1.0))
    return {
        "asset_path": str(asset_path),
        "bounding_box": bounding_box,
        "semantic_score": float(score if score is not None else 1.0),
    }


def candidates_from_hits(hits: list[Any]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    candidates: list[dict[str, Any]] = []
    skipped: list[dict[str, Any]] = []
    for index, hit in enumerate(hits):
        candidate = candidate_from_hit(hit)
        if candidate is None:
            skipped.append({"index": index, "reason": "missing asset_path or bounding_box"})
            continue
        candidates.append(candidate)
    return candidates, skipped


def auto_query(text: str | None, semantic_result: dict[str, Any]) -> str:
    explicit_text = str(text or "").strip()
    if explicit_text:
        return explicit_text

    semantic = semantic_result.get("semantic") if isinstance(semantic_result, dict) else None
    actor = semantic_result.get("actor") if isinstance(semantic_result, dict) else None
    parts: list[str] = []
    if isinstance(semantic, dict):
        tags = semantic.get("tags")
        if isinstance(tags, list):
            parts.extend(str(tag).strip() for tag in tags if str(tag).strip())
    if not parts and isinstance(actor, dict):
        label = str(actor.get("label") or "").strip()
        if label:
            parts.append(label)
    return " ".join(parts).strip()


def bounded_int(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, int(value)))


def build_clip_filters(
    project_names: list[str] | None = None,
    asset_types: list[str] | None = None,
    filters: dict[str, Any] | None = None,
) -> dict[str, Any]:
    cfg = clip_config()
    merged: dict[str, Any] = dict(filters or {})
    if project_names is None and "project_names" not in merged and cfg["project_name"]:
        merged["project_names"] = [cfg["project_name"]]
    elif project_names is not None:
        merged["project_names"] = project_names
    if asset_types is not None:
        merged["asset_types"] = asset_types
    return {key: value for key, value in merged.items() if value not in (None, [], {})}


# Backward-compatible aliases used by the external MCP server.
_clip_config = clip_config
_clip_api_url = clip_api_url
_clip_request_json = clip_request_json
_clip_error = clip_error
_default_clip_output_fields = default_clip_output_fields
_clip_search_assets = clip_search_assets
_clip_search_assets_by_image = clip_search_assets_by_image
_dinov3_search_assets_by_image = dinov3_search_assets_by_image
_solver_settings_payload = solver_settings_payload
_hit_field = hit_field
_candidate_from_hit = candidate_from_hit
_candidates_from_hits = candidates_from_hits
_auto_query = auto_query
_bounded_int = bounded_int
_build_clip_filters = build_clip_filters
