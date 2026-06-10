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


def reset_collection(collection_name, schema):
    status, _ = request_json("DELETE", "/milvus/collections/{0}".format(collection_name), allow_404=True)
    if status == 404:
        unreal.log("[SceneAssembly] Collection not found, will create: {0}".format(collection_name))
    else:
        unreal.log("[SceneAssembly] Dropped collection: {0}".format(collection_name))

    _, envelope = request_json("POST", "/milvus/collections", schema)
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


def flush_batches(rows, collection_name, force=False, failed=None):
    while rows and (force or len(rows) >= config.INGEST_BATCH_SIZE):
        batch = rows[: config.INGEST_BATCH_SIZE]
        try:
            ingest_batch(collection_name, batch)
        except Exception as exc:
            record_failed_rows(failed, collection_name, batch, exc)
        del rows[: len(batch)]


def record_failed_asset(failed, asset_path, exc):
    failed.append({"asset_path": asset_path, "error": str(exc)})
    unreal.log_error("[SceneAssembly] Failed asset {0}: {1}".format(asset_path, exc))
    unreal.log_error(traceback.format_exc())
