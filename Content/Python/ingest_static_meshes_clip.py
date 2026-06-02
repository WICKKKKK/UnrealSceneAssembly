import json
import time

import unreal

import config
import ingest_static_meshes as common


RESERVED_OUTPUT_FIELDS = common.RESERVED_OUTPUT_FIELDS
VERIFY_OUTPUT_FIELDS = RESERVED_OUTPUT_FIELDS + ["bounding_box"]
DATA_URI_PREFIX = "data:image/png;base64,"
CLIP_IMAGE_EMBED_API_LIMIT = 16


def _clip_batch_size():
    configured = int(getattr(config, "CLIP_EMBED_BATCH_SIZE", CLIP_IMAGE_EMBED_API_LIMIT))
    return max(1, min(CLIP_IMAGE_EMBED_API_LIMIT, configured))


def clip_model_info():
    _, envelope = common.request_json("GET", "/clip/model")
    data = common.unwrap_data(envelope)
    if not isinstance(data, dict):
        raise RuntimeError("Unexpected CLIP model response: {0}".format(data))

    dimension = data.get("dimension")
    unreal.log(
        "[SceneAssembly] CLIP model={0}, dim={1}, backend={2}, device={3}".format(
            data.get("model_name"),
            dimension,
            data.get("backend"),
            data.get("device"),
        )
    )
    if dimension != config.CLIP_DIMENSION:
        raise RuntimeError("Unexpected CLIP dimension: expected {0}, got {1}".format(config.CLIP_DIMENSION, dimension))
    return data


def collection_schema(collection_name, dimension):
    return {
        "collection_name": collection_name,
        "dimension": dimension,
        "metric_type": "COSINE",
        "provider": "clip",
        "category": "assets",
        "enable_dynamic_field": True,
        "extra": {"created_by": "SceneAssembly", "kind": "clip_assets"},
    }


def reset_collection(collection_name):
    status, _ = common.request_json("DELETE", "/milvus/collections/{0}".format(collection_name), allow_404=True)
    if status == 404:
        unreal.log("[SceneAssembly] Collection not found, will create: {0}".format(collection_name))
    else:
        unreal.log("[SceneAssembly] Dropped collection: {0}".format(collection_name))

    _, envelope = common.request_json("POST", "/milvus/collections", collection_schema(collection_name, config.CLIP_DIMENSION))
    common.unwrap_data(envelope)
    unreal.log("[SceneAssembly] Created CLIP collection: {0}".format(collection_name))


def _thumbnail_data_uri(thumbnail_path):
    return DATA_URI_PREFIX + common.encode_thumbnail_base64(thumbnail_path)


def validate_embedding_result(result, context):
    if not isinstance(result, dict):
        raise RuntimeError("Unexpected {0}: {1}".format(context, result))
    if result.get("dimension") != config.CLIP_DIMENSION:
        raise RuntimeError("Unexpected CLIP dimension in {0}: {1}".format(context, result.get("dimension")))
    vector = result.get("vector")
    if not isinstance(vector, list) or len(vector) != config.CLIP_DIMENSION:
        raise RuntimeError("CLIP response has invalid vector in {0}".format(context))
    return vector


def _parse_embedding_results(data, expected_count):
    if not isinstance(data, dict):
        raise RuntimeError("Unexpected CLIP embed response: {0}".format(data))
    embeddings = data.get("embeddings")
    if not isinstance(embeddings, list):
        raise RuntimeError("CLIP embed response missing embeddings")
    if len(embeddings) != expected_count:
        raise RuntimeError("CLIP embed result count mismatch: expected {0}, got {1}".format(expected_count, len(embeddings)))

    vectors = [None] * expected_count
    for fallback_index, result in enumerate(embeddings):
        result_index = result.get("index") if isinstance(result, dict) else None
        index = fallback_index if result_index is None else int(result_index)
        if index < 0 or index >= expected_count:
            raise RuntimeError("CLIP embed result index out of range: {0}".format(index))
        if vectors[index] is not None:
            raise RuntimeError("CLIP embed response contains duplicate index: {0}".format(index))
        vectors[index] = validate_embedding_result(result, "CLIP embed result[{0}]".format(index))

    missing = [index for index, vector in enumerate(vectors) if vector is None]
    if missing:
        raise RuntimeError("CLIP embed response missing indexes: {0}".format(missing))
    return vectors


def embed_images_batch(thumbnail_paths):
    if not thumbnail_paths:
        return []
    if len(thumbnail_paths) > CLIP_IMAGE_EMBED_API_LIMIT:
        raise RuntimeError("CLIP image embed batch exceeds API limit: {0}".format(len(thumbnail_paths)))

    payload = {"images": [{"url": _thumbnail_data_uri(path)} for path in thumbnail_paths]}
    _, envelope = common.request_json(
        "POST",
        "/clip/embed/image",
        payload,
        timeout=getattr(config, "EMBED_BATCH_TIMEOUT_SECONDS", config.HTTP_TIMEOUT_SECONDS),
    )
    data = common.unwrap_data(envelope)
    return _parse_embedding_results(data, len(thumbnail_paths))


def embed_image(thumbnail_path):
    return embed_images_batch([thumbnail_path])[0]


def ingest_batch(collection_name, rows):
    return common.ingest_batch(collection_name, rows)


def flush_batches(rows, force=False, failed=None):
    while rows and (force or len(rows) >= config.INGEST_BATCH_SIZE):
        batch = rows[: config.INGEST_BATCH_SIZE]
        try:
            ingest_batch(config.COLLECTION_CLIP, batch)
        except Exception as exc:
            common.record_failed_rows(failed, config.COLLECTION_CLIP, batch, exc)
        del rows[: len(batch)]


def _component(vector, name, fallback_index):
    if hasattr(vector, name):
        return float(getattr(vector, name))
    upper_name = name.upper()
    if hasattr(vector, upper_name):
        return float(getattr(vector, upper_name))
    return float(vector[fallback_index])


def _vector_xyz(vector):
    return {
        "X": _component(vector, "x", 0),
        "Y": _component(vector, "y", 1),
        "Z": _component(vector, "z", 2),
    }


def _bbox_from_bounds(bounds):
    origin = getattr(bounds, "origin", None)
    extent = getattr(bounds, "box_extent", None)
    if origin is None or extent is None:
        return None
    return {
        "BboxSize": {
            "X": _component(extent, "x", 0) * 2.0,
            "Y": _component(extent, "y", 1) * 2.0,
            "Z": _component(extent, "z", 2) * 2.0,
        },
        "BboxCenter": _vector_xyz(origin),
    }


def _bbox_from_box(box):
    min_point = getattr(box, "min", None)
    max_point = getattr(box, "max", None)
    if min_point is None or max_point is None:
        return None

    min_xyz = _vector_xyz(min_point)
    max_xyz = _vector_xyz(max_point)
    return {
        "BboxSize": {
            "X": max_xyz["X"] - min_xyz["X"],
            "Y": max_xyz["Y"] - min_xyz["Y"],
            "Z": max_xyz["Z"] - min_xyz["Z"],
        },
        "BboxCenter": {
            "X": (min_xyz["X"] + max_xyz["X"]) * 0.5,
            "Y": (min_xyz["Y"] + max_xyz["Y"]) * 0.5,
            "Z": (min_xyz["Z"] + max_xyz["Z"]) * 0.5,
        },
    }


def asset_bounding_box(asset_data, asset_path):
    try:
        mesh = None
        if hasattr(asset_data, "get_asset"):
            mesh = asset_data.get_asset()
        if mesh is None:
            mesh = unreal.load_asset(asset_path)
        if mesh is None:
            raise RuntimeError("failed to load StaticMesh asset")

        if hasattr(mesh, "get_bounds"):
            bounding_box = _bbox_from_bounds(mesh.get_bounds())
            if bounding_box is not None:
                return bounding_box
        if hasattr(mesh, "get_bounding_box"):
            bounding_box = _bbox_from_box(mesh.get_bounding_box())
            if bounding_box is not None:
                return bounding_box
        raise RuntimeError("StaticMesh bounds API is unavailable")
    except Exception as exc:
        unreal.log_warning("[SceneAssembly] Failed to read bounding_box for {0}: {1}".format(asset_path, exc))
        return None


def build_row(asset_id, asset_name, asset_path, thumbnail_url, public_path, vector, bounding_box=None):
    fields = {
        "thumbnail_url": thumbnail_url,
        "asset_name": asset_name,
        "asset_path": asset_path,
        "asset_type": "StaticMesh",
        "public_path": public_path,
    }
    if bounding_box is not None:
        fields["bounding_box"] = bounding_box
    return {
        "project_name": config.PROJECT_NAME,
        "asset_id": asset_id,
        "vector": vector,
        "fields": fields,
    }


def add_row_from_vector(item, vector, rows):
    rows.append(
        build_row(
            item["asset_id"],
            item["asset_name"],
            item["asset_path"],
            item["thumbnail_url"],
            item["public_path"],
            vector,
            item.get("bounding_box"),
        )
    )


def flush_pending_embeddings(pending, rows, failed):
    if not pending:
        return 0

    unreal.log("[SceneAssembly] CLIP embedding batch: {0} thumbnails".format(len(pending)))
    succeeded = 0
    try:
        vectors = embed_images_batch([item["out_path"] for item in pending])
    except Exception as exc:
        unreal.log_warning(
            "[SceneAssembly] CLIP batch embedding failed, falling back to single-image embedding: {0}".format(exc)
        )
        for item in pending:
            try:
                vector = embed_image(item["out_path"])
                add_row_from_vector(item, vector, rows)
                succeeded += 1
            except Exception as item_exc:
                common.record_failed_asset(failed, item["asset_path"], item_exc)
        pending[:] = []
        return succeeded

    for item, vector in zip(pending, vectors):
        try:
            add_row_from_vector(item, vector, rows)
            succeeded += 1
        except Exception as exc:
            common.record_failed_asset(failed, item["asset_path"], exc)

    pending[:] = []
    return succeeded


def ingest_static_meshes_clip():
    started_at = time.time()
    common.auth_self_check()
    clip_model_info()
    reset_collection(config.COLLECTION_CLIP)

    asset_data_list = common.enumerate_static_mesh_assets()
    total = len(asset_data_list)
    unreal.log("[SceneAssembly] Found {0} StaticMesh assets for CLIP ingest.".format(total))

    rows = []
    pending = []
    succeeded = 0
    failed = []
    embed_batch_size = _clip_batch_size()

    for index, asset_data in enumerate(asset_data_list, start=1):
        asset_id, asset_name, asset_path = common.asset_identity(asset_data)
        out_path, public_path, thumbnail_url = common.thumbnail_paths(asset_id)
        try:
            unreal.log("[SceneAssembly] [{0}/{1}] {2}".format(index, total, asset_path))
            bounding_box = asset_bounding_box(asset_data, asset_path)
            if not common.export_thumbnail(asset_path, out_path):
                raise RuntimeError("thumbnail export returned false")
            pending.append(
                {
                    "asset_id": asset_id,
                    "asset_name": asset_name,
                    "asset_path": asset_path,
                    "out_path": out_path,
                    "thumbnail_url": thumbnail_url,
                    "public_path": public_path,
                    "bounding_box": bounding_box,
                }
            )
        except Exception as exc:
            common.record_failed_asset(failed, asset_path, exc)
            continue

        if len(pending) >= embed_batch_size:
            succeeded += flush_pending_embeddings(pending, rows, failed)
            flush_batches(rows, failed=failed)

    succeeded += flush_pending_embeddings(pending, rows, failed)
    flush_batches(rows, failed=failed)

    flush_batches(rows, force=True, failed=failed)
    elapsed = time.time() - started_at
    unreal.log(
        "[SceneAssembly] CLIP ingest finished. total={0}, succeeded={1}, failed={2}, elapsed={3:.1f}s".format(
            total,
            succeeded,
            len(failed),
            elapsed,
        )
    )
    for item in failed:
        unreal.log_warning("[SceneAssembly] Failed: {0} | {1}".format(item["asset_path"], item["error"]))
    return {"total": total, "succeeded": succeeded, "failed": failed, "elapsed_seconds": elapsed}


def verify_query(limit=10):
    payload = {
        "filters": {"project_names": [config.PROJECT_NAME]},
        "output_fields": VERIFY_OUTPUT_FIELDS,
        "limit": limit,
        "offset": 0,
    }
    _, envelope = common.request_json("POST", "/milvus/collections/{0}/query_assets".format(config.COLLECTION_CLIP), payload)
    data = common.unwrap_data(envelope)
    unreal.log("[SceneAssembly] CLIP query verification: {0}".format(json.dumps(data, ensure_ascii=False)[:2000]))
    return data


def verify_search_text(text, limit=5):
    payload = {
        "text": text,
        "limit": limit,
        "filters": {"project_names": [config.PROJECT_NAME]},
        "output_fields": VERIFY_OUTPUT_FIELDS,
    }
    _, envelope = common.request_json(
        "POST",
        "/clip/collections/{0}/search/single_text".format(config.COLLECTION_CLIP),
        payload,
        timeout=getattr(config, "HTTP_TIMEOUT_SECONDS", 120.0),
    )
    data = common.unwrap_data(envelope)
    unreal.log("[SceneAssembly] CLIP text search verification: {0}".format(json.dumps(data, ensure_ascii=False)[:2000]))
    return data


if __name__ == "__main__":
    ingest_static_meshes_clip()
