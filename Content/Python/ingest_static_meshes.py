import base64
import hashlib
import json
import os
import time
import traceback
import urllib.error
import urllib.request

import unreal

import config


RESERVED_OUTPUT_FIELDS = ["thumbnail_url", "asset_name", "asset_path", "asset_type", "public_path"]


class ApiError(RuntimeError):
    def __init__(self, method, url, status, body):
        super().__init__("{0} {1} failed with HTTP {2}: {3}".format(method, url, status, body))
        self.method = method
        self.url = url
        self.status = status
        self.body = body


def _api_url(path):
    return "{0}{1}{2}".format(config.BASE_URL.rstrip("/"), config.API_PREFIX.rstrip("/"), path)


def _headers():
    return {
        "Authorization": "Bearer {0}".format(config.API_KEY),
        "Content-Type": "application/json",
        "Accept": "application/json",
    }


def _decode_json(data):
    if not data:
        return None
    return json.loads(data.decode("utf-8"))


def request_json(method, path, payload=None, timeout=None, allow_404=False):
    url = _api_url(path)
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    attempts = max(1, int(getattr(config, "HTTP_MAX_RETRIES", 1)))
    retry_delay = float(getattr(config, "HTTP_RETRY_DELAY_SECONDS", 1.0))
    for attempt in range(1, attempts + 1):
        request = urllib.request.Request(url, data=body, headers=_headers(), method=method)
        try:
            with urllib.request.urlopen(request, timeout=timeout or config.HTTP_TIMEOUT_SECONDS) as response:
                return response.status, _decode_json(response.read())
        except urllib.error.HTTPError as exc:
            response_body = exc.read().decode("utf-8", errors="replace")
            if allow_404 and exc.code == 404:
                return exc.code, None
            if exc.code in (408, 409, 425, 429, 500, 502, 503, 504) and attempt < attempts:
                delay = retry_delay * attempt
                unreal.log_warning(
                    "[SceneAssembly] {0} {1} returned HTTP {2}; retry {3}/{4} after {5:.1f}s".format(
                        method,
                        url,
                        exc.code,
                        attempt + 1,
                        attempts,
                        delay,
                    )
                )
                time.sleep(delay)
                continue
            raise ApiError(method, url, exc.code, response_body)
        except (urllib.error.URLError, TimeoutError) as exc:
            if attempt < attempts:
                delay = retry_delay * attempt
                unreal.log_warning(
                    "[SceneAssembly] {0} {1} connection failed: {2}; retry {3}/{4} after {5:.1f}s".format(
                        method,
                        url,
                        exc,
                        attempt + 1,
                        attempts,
                        delay,
                    )
                )
                time.sleep(delay)
                continue
            raise RuntimeError("{0} {1} failed: {2}".format(method, url, exc))


def unwrap_data(envelope):
    if envelope is None:
        return None
    if not isinstance(envelope, dict):
        raise RuntimeError("Unexpected non-object response: {0}".format(envelope))
    code = envelope.get("code")
    if code not in (0, 200, None):
        raise RuntimeError("Backend returned code={0}, message={1}".format(code, envelope.get("message")))
    return envelope.get("data")


def auth_self_check():
    _, envelope = request_json("GET", "/auth/me")
    data = unwrap_data(envelope)
    principal_type = data.get("principal_type") if isinstance(data, dict) else None
    if principal_type in (None, "anonymous"):
        raise RuntimeError("Authentication failed: principal_type={0}".format(principal_type))
    unreal.log("[SceneAssembly] Authenticated as {0}".format(data.get("display_name") or data.get("username") or principal_type))


def dinov3_info():
    _, envelope = request_json("GET", "/dinov3/info")
    data = unwrap_data(envelope)
    if isinstance(data, dict):
        unreal.log("[SceneAssembly] DINOv3 model={0}, dim={1}, ready={2}".format(data.get("model"), data.get("dim"), data.get("ready")))
    return data


def collection_schema(collection_name):
    common_fields = [
        {"name": "pk", "data_type": "INT64", "is_primary": True, "auto_id": True},
        {"name": "asset_id", "data_type": "VARCHAR", "params": {"max_length": 128}},
        {"name": "project_name", "data_type": "VARCHAR", "params": {"max_length": 128}, "is_partition_key": True},
    ]
    fields = common_fields + [
        {"name": "cls_embedding", "data_type": "FLOAT_VECTOR", "params": {"dim": config.DINO_DIMENSION}},
        {
            "name": "patches",
            "data_type": "ARRAY",
            "element_type": "STRUCT",
            "max_capacity": 256,
            "struct_fields": [
                {"name": "patch_idx", "data_type": "INT16"},
                {"name": "patch_x", "data_type": "INT8"},
                {"name": "patch_y", "data_type": "INT8"},
                {"name": "embedding", "data_type": "FLOAT_VECTOR", "params": {"dim": config.DINO_DIMENSION}},
            ],
        },
    ]
    indexes = [
        {
            "field_name": "cls_embedding",
            "index_name": "cls_idx",
            "index_type": "HNSW",
            "metric_type": "IP",
            "params": {"M": 16, "efConstruction": 256},
        },
        {
            "field_name": "patches[embedding]",
            "index_name": "patch_emb_idx",
            "index_type": "HNSW",
            "metric_type": "MAX_SIM_IP",
            "params": {"M": 16, "efConstruction": 256},
        },
    ]

    return {
        "collection_name": collection_name,
        "dimension": config.DINO_DIMENSION,
        "metric_type": "IP",
        "provider": "dinov3",
        "category": "assets",
        "auto_id": True,
        "enable_dynamic_field": True,
        "fields": fields,
        "indexes": indexes,
        "extra": {"created_by": "SceneAssembly", "kind": "dinov3_assets"},
    }


def reset_collection(collection_name):
    status, _ = request_json("DELETE", "/milvus/collections/{0}".format(collection_name), allow_404=True)
    if status == 404:
        unreal.log("[SceneAssembly] Collection not found, will create: {0}".format(collection_name))
    else:
        unreal.log("[SceneAssembly] Dropped collection: {0}".format(collection_name))
    _, envelope = request_json("POST", "/milvus/collections", collection_schema(collection_name))
    unwrap_data(envelope)
    unreal.log("[SceneAssembly] Created collection: {0}".format(collection_name))


def enumerate_static_mesh_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        class_path = unreal.TopLevelAssetPath("/Script/Engine.StaticMesh")
    except TypeError:
        class_path = unreal.TopLevelAssetPath("/Script/Engine", "StaticMesh")
    assets = registry.get_assets_by_class(class_path, True)
    content_assets = [asset_data for asset_data in assets if is_content_asset(asset_data)]
    skipped = len(assets) - len(content_assets)
    if skipped > 0:
        unreal.log("[SceneAssembly] Skipped {0} non-Content StaticMesh assets.".format(skipped))
    return sorted(content_assets, key=lambda asset_data: soft_object_path_string(asset_data))


def path_matches_prefix(path, prefix):
    normalized = prefix.rstrip("/")
    return path == normalized or path.startswith(normalized + "/") or path.startswith(normalized + ".")


def is_content_asset(asset_data):
    asset_path = soft_object_path_string(asset_data)
    package_name = str(getattr(asset_data, "package_name", ""))
    for prefix in config.ASSET_PATH_PREFIXES:
        if path_matches_prefix(asset_path, prefix) or path_matches_prefix(package_name, prefix):
            return True
    return False


def soft_object_path_string(asset_data):
    get_soft_object_path = getattr(asset_data, "get_soft_object_path", None)
    if callable(get_soft_object_path):
        soft_path = get_soft_object_path()
        to_string = getattr(soft_path, "to_string", None)
        if callable(to_string):
            return to_string()
        path = str(soft_path)
        if path and path != "None":
            return path

    object_path = getattr(asset_data, "object_path", None)
    if object_path is not None:
        path = str(object_path)
        if path and path != "None":
            return path

    package_name = str(asset_data.package_name)
    asset_name = str(asset_data.asset_name)
    return "{0}.{1}".format(package_name, asset_name)


def asset_identity(asset_data):
    asset_path = soft_object_path_string(asset_data)
    asset_name = str(asset_data.asset_name)
    asset_id = hashlib.md5(asset_path.encode("utf-8")).hexdigest()
    return asset_id, asset_name, asset_path


def thumbnail_paths(asset_id):
    filename = "{0}.png".format(asset_id)
    out_path = os.path.join(config.THUMB_DIR, filename)
    public_path = "{0}/{1}".format(config.THUMB_REL.strip("/"), filename)
    thumbnail_url = "{0}/{1}".format(config.THUMB_URL_PREFIX.rstrip("/"), filename)
    return out_path, public_path, thumbnail_url


def export_thumbnail(asset_path, out_path):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    return unreal.ThumbnailLibrary.export_asset_thumbnail(asset_path, out_path, config.THUMB_RESOLUTION)


def encode_thumbnail_base64(thumbnail_path):
    with open(thumbnail_path, "rb") as image_file:
        return base64.b64encode(image_file.read()).decode("ascii")


def validate_embed_data(data, context):
    if not isinstance(data, dict):
        raise RuntimeError("Unexpected {0}: {1}".format(context, data))
    if data.get("dim") != config.DINO_DIMENSION:
        raise RuntimeError("Unexpected DINOv3 dim in {0}: {1}".format(context, data.get("dim")))
    if not data.get("cls_embedding"):
        raise RuntimeError("DINOv3 response missing cls_embedding in {0}".format(context))
    if not data.get("patch_embeddings"):
        raise RuntimeError("DINOv3 response missing patch_embeddings in {0}".format(context))
    return data


def embed_thumbnail(thumbnail_path):
    payload = {
        "image": {"base64": encode_thumbnail_base64(thumbnail_path)},
        "return_patches": True,
        "l2_normalize": True,
    }
    _, envelope = request_json("POST", "/dinov3/embed", payload, timeout=config.HTTP_TIMEOUT_SECONDS)
    data = unwrap_data(envelope)
    return validate_embed_data(data, "DINOv3 embed response")


def embed_thumbnails_batch(thumbnail_paths):
    if not thumbnail_paths:
        return []

    payload = {
        "images": [{"base64": encode_thumbnail_base64(path)} for path in thumbnail_paths],
        "return_patches": True,
        "l2_normalize": True,
    }
    _, envelope = request_json(
        "POST",
        "/dinov3/embed/batch",
        payload,
        timeout=config.EMBED_BATCH_TIMEOUT_SECONDS,
    )
    data = unwrap_data(envelope)
    if not isinstance(data, dict):
        raise RuntimeError("Unexpected DINOv3 batch response: {0}".format(data))
    results = data.get("results")
    if not isinstance(results, list):
        raise RuntimeError("DINOv3 batch response missing results")
    if len(results) != len(thumbnail_paths):
        raise RuntimeError("DINOv3 batch result count mismatch: expected {0}, got {1}".format(len(thumbnail_paths), len(results)))
    return [validate_embed_data(result, "DINOv3 batch result[{0}]".format(index)) for index, result in enumerate(results)]


def build_patch_structs(embed_data):
    patch_grid = embed_data.get("patch_grid") or {}
    grid_w = int(patch_grid.get("w") or 0)
    if grid_w <= 0:
        raise RuntimeError("Invalid patch_grid in embed response: {0}".format(patch_grid))

    patches = []
    for index, embedding in enumerate(embed_data["patch_embeddings"]):
        patches.append(
            {
                "patch_idx": index,
                "patch_x": index % grid_w,
                "patch_y": index // grid_w,
                "embedding": embedding,
            }
        )
    return patches


def ingest_batch(collection_name, rows):
    if not rows:
        return {"total": 0, "succeeded": 0, "failed": 0, "errors": []}
    _, envelope = request_json(
        "POST",
        "/milvus/collections/{0}/assets".format(collection_name),
        {"assets": rows},
        timeout=getattr(config, "INGEST_TIMEOUT_SECONDS", config.HTTP_TIMEOUT_SECONDS),
    )
    data = unwrap_data(envelope)
    if isinstance(data, dict):
        unreal.log(
            "[SceneAssembly] Insert {0}: total={1}, succeeded={2}, failed={3}".format(
                collection_name,
                data.get("total"),
                data.get("succeeded"),
                data.get("failed"),
            )
        )
        for error in data.get("errors") or []:
            unreal.log_warning("[SceneAssembly] Insert error in {0}: {1}".format(collection_name, error))
    return data


def record_failed_rows(failed, collection_name, rows, exc):
    unreal.log_error("[SceneAssembly] Insert batch failed in {0}: {1}".format(collection_name, exc))
    unreal.log_error(traceback.format_exc())
    if failed is None:
        raise exc
    for row in rows:
        fields = row.get("fields") or {}
        failed.append(
            {
                "asset_path": fields.get("asset_path") or row.get("asset_id") or "<unknown>",
                "error": "insert {0} failed: {1}".format(collection_name, exc),
            }
        )


def flush_batches(rows, force=False, failed=None):
    while rows and (force or len(rows) >= config.INGEST_BATCH_SIZE):
        batch = rows[: config.INGEST_BATCH_SIZE]
        try:
            ingest_batch(config.COLLECTION, batch)
        except Exception as exc:
            record_failed_rows(failed, config.COLLECTION, batch, exc)
        del rows[: len(batch)]


def build_row(asset_id, asset_name, asset_path, thumbnail_url, public_path, embed_data):
    fields = {
        "thumbnail_url": thumbnail_url,
        "asset_name": asset_name,
        "asset_path": asset_path,
        "asset_type": "StaticMesh",
        "public_path": public_path,
    }
    return {
        "project_name": config.PROJECT_NAME,
        "asset_id": asset_id,
        "cls_embedding": embed_data["cls_embedding"],
        "patches": build_patch_structs(embed_data),
        "fields": fields,
    }


def add_row_from_embed(item, embed_data, rows):
    rows.append(
        build_row(
            item["asset_id"],
            item["asset_name"],
            item["asset_path"],
            item["thumbnail_url"],
            item["public_path"],
            embed_data,
        )
    )


def record_failed_asset(failed, asset_path, exc):
    failed.append({"asset_path": asset_path, "error": str(exc)})
    unreal.log_error("[SceneAssembly] Failed asset {0}: {1}".format(asset_path, exc))
    unreal.log_error(traceback.format_exc())


def flush_pending_embeddings(pending, rows, failed):
    if not pending:
        return 0

    unreal.log("[SceneAssembly] Embedding batch: {0} thumbnails".format(len(pending)))
    succeeded = 0
    try:
        embed_results = embed_thumbnails_batch([item["out_path"] for item in pending])
    except Exception as exc:
        unreal.log_warning(
            "[SceneAssembly] Batch embedding failed, falling back to single-image embedding: {0}".format(exc)
        )
        for item in pending:
            try:
                embed_data = embed_thumbnail(item["out_path"])
                add_row_from_embed(item, embed_data, rows)
                succeeded += 1
            except Exception as item_exc:
                record_failed_asset(failed, item["asset_path"], item_exc)
        pending[:] = []
        return succeeded

    for item, embed_data in zip(pending, embed_results):
        try:
            add_row_from_embed(item, embed_data, rows)
            succeeded += 1
        except Exception as exc:
            record_failed_asset(failed, item["asset_path"], exc)

    pending[:] = []
    return succeeded


def ingest_static_meshes():
    started_at = time.time()
    auth_self_check()
    dinov3_info()
    reset_collection(config.COLLECTION)

    asset_data_list = enumerate_static_mesh_assets()
    total = len(asset_data_list)
    unreal.log("[SceneAssembly] Found {0} StaticMesh assets.".format(total))

    rows = []
    pending = []
    succeeded = 0
    failed = []

    for index, asset_data in enumerate(asset_data_list, start=1):
        asset_id, asset_name, asset_path = asset_identity(asset_data)
        out_path, public_path, thumbnail_url = thumbnail_paths(asset_id)
        try:
            unreal.log("[SceneAssembly] [{0}/{1}] {2}".format(index, total, asset_path))
            if not export_thumbnail(asset_path, out_path):
                raise RuntimeError("thumbnail export returned false")
            pending.append(
                {
                    "asset_id": asset_id,
                    "asset_name": asset_name,
                    "asset_path": asset_path,
                    "out_path": out_path,
                    "thumbnail_url": thumbnail_url,
                    "public_path": public_path,
                }
            )
        except Exception as exc:
            record_failed_asset(failed, asset_path, exc)
            continue

        if len(pending) >= config.EMBED_BATCH_SIZE:
            succeeded += flush_pending_embeddings(pending, rows, failed)
            flush_batches(rows, failed=failed)

    succeeded += flush_pending_embeddings(pending, rows, failed)
    flush_batches(rows, failed=failed)

    flush_batches(rows, force=True, failed=failed)
    elapsed = time.time() - started_at
    unreal.log("[SceneAssembly] Finished. total={0}, succeeded={1}, failed={2}, elapsed={3:.1f}s".format(total, succeeded, len(failed), elapsed))
    for item in failed:
        unreal.log_warning("[SceneAssembly] Failed: {0} | {1}".format(item["asset_path"], item["error"]))
    return {"total": total, "succeeded": succeeded, "failed": failed, "elapsed_seconds": elapsed}


def verify_query(limit=10):
    payload = {
        "filters": {"project_names": [config.PROJECT_NAME]},
        "output_fields": RESERVED_OUTPUT_FIELDS,
        "limit": limit,
        "offset": 0,
    }
    _, envelope = request_json("POST", "/milvus/collections/{0}/query_assets".format(config.COLLECTION), payload)
    data = unwrap_data(envelope)
    unreal.log("[SceneAssembly] Query verification: {0}".format(json.dumps(data, ensure_ascii=False)[:2000]))
    return data


if __name__ == "__main__":
    ingest_static_meshes()
