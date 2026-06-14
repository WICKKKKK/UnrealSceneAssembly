import base64
import hashlib
import json
import os
import time
import traceback
import urllib.error
import urllib.request
import uuid

import unreal

import config


RESERVED_OUTPUT_FIELDS = ["thumbnail_url", "asset_name", "asset_path", "asset_type", "orient_thumbnail_url", "thumbnail_camera"]


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


def _auth_headers(accept="application/json"):
    return {
        "Authorization": "Bearer {0}".format(config.API_KEY),
        "Accept": accept,
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


def _public_url(relative_path):
    normalized = str(relative_path or "").replace("\\", "/").lstrip("/")
    if not normalized:
        return ""
    return "/public/{0}".format(normalized)


def absolute_public_url(url_or_path):
    value = str(url_or_path or "").strip()
    if not value:
        return ""
    lower = value.lower()
    if lower.startswith("http://") or lower.startswith("https://") or lower.startswith("data:"):
        return value
    return "{0}/{1}".format(config.BASE_URL.rstrip("/"), value.lstrip("/"))


def _multipart_upload_body(file_paths, target_dir, conflict_strategy):
    boundary = "----SceneAssemblyBoundary{0}".format(uuid.uuid4().hex)
    chunks = []

    def add_form_field(name, value):
        chunks.append("--{0}\r\n".format(boundary).encode("ascii"))
        chunks.append('Content-Disposition: form-data; name="{0}"\r\n\r\n'.format(name).encode("ascii"))
        chunks.append(str(value).encode("utf-8"))
        chunks.append(b"\r\n")

    add_form_field("target_dir", target_dir)
    add_form_field("conflict_strategy", conflict_strategy)
    for file_path in file_paths:
        filename = os.path.basename(file_path)
        chunks.append("--{0}\r\n".format(boundary).encode("ascii"))
        chunks.append(
            'Content-Disposition: form-data; name="files"; filename="{0}"\r\n'.format(filename).encode("utf-8")
        )
        chunks.append(b"Content-Type: image/png\r\n\r\n")
        with open(file_path, "rb") as image_file:
            chunks.append(image_file.read())
        chunks.append(b"\r\n")
    chunks.append("--{0}--\r\n".format(boundary).encode("ascii"))
    return boundary, b"".join(chunks)


def upload_files_batch(file_paths, target_dir=None, conflict_strategy="overwrite", timeout=None):
    paths = [path for path in file_paths if path]
    if not paths:
        return {}
    missing = [path for path in paths if not os.path.isfile(path)]
    if missing:
        raise RuntimeError("Upload files missing: {0}".format(", ".join(missing)))

    boundary, body = _multipart_upload_body(
        paths,
        target_dir if target_dir is not None else config.THUMB_REL,
        conflict_strategy,
    )
    headers = _auth_headers()
    headers["Content-Type"] = "multipart/form-data; boundary={0}".format(boundary)
    request = urllib.request.Request(
        _api_url("/files/upload/batch"),
        data=body,
        headers=headers,
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout or config.HTTP_TIMEOUT_SECONDS) as response:
            envelope = _decode_json(response.read())
    except urllib.error.HTTPError as exc:
        response_body = exc.read().decode("utf-8", errors="replace")
        raise ApiError("POST", _api_url("/files/upload/batch"), exc.code, response_body)
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError("POST {0} failed: {1}".format(_api_url("/files/upload/batch"), exc))

    data = unwrap_data(envelope)
    results = data.get("results") if isinstance(data, dict) else None
    if not isinstance(results, list):
        raise RuntimeError("Unexpected upload batch response: {0}".format(data))

    output = {}
    for result in results:
        if not isinstance(result, dict):
            continue
        filename = result.get("filename")
        relative_path = result.get("relative_path")
        if filename and relative_path:
            output[str(filename)] = str(relative_path).replace("\\", "/").lstrip("/")
    expected = {os.path.basename(path) for path in paths}
    missing_results = sorted(filename for filename in expected if filename not in output)
    if missing_results:
        raise RuntimeError("Upload batch response missing files: {0}".format(", ".join(missing_results)))
    return output


def apply_uploaded_thumbnail_urls(items, upload_results):
    for item in items:
        filename = os.path.basename(item["out_path"])
        relative_path = upload_results.get(filename)
        if not relative_path:
            raise RuntimeError("Upload response missing thumbnail: {0}".format(filename))
        item["thumbnail_url"] = _public_url(relative_path)


def apply_uploaded_orient_urls(items, upload_results):
    for item in items:
        orient_out_path = item.get("orient_out_path")
        if not orient_out_path:
            continue
        filename = os.path.basename(orient_out_path)
        relative_path = upload_results.get(filename)
        if not relative_path:
            raise RuntimeError("Upload response missing orient thumbnail: {0}".format(filename))
        item["orient_thumbnail_url"] = _public_url(relative_path)


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


def asset_path_no_suffix(path):
    value = str(path or "").strip()
    if not value or value == "None":
        return ""
    slash_index = value.rfind("/")
    dot_index = value.find(".", slash_index + 1)
    if dot_index < 0:
        return value
    return value[:dot_index]


def object_path_from_asset_path(asset_path):
    value = str(asset_path or "").strip()
    if not value or value == "None":
        return ""
    slash_index = value.rfind("/")
    if value.find(".", slash_index + 1) >= 0:
        return value
    asset_name = value[slash_index + 1 :] if slash_index >= 0 else value
    return "{0}.{1}".format(value, asset_name) if asset_name else value


def asset_identity(asset_data):
    object_path = soft_object_path_string(asset_data)
    asset_path = asset_path_no_suffix(object_path)
    asset_name = str(asset_data.asset_name)
    asset_id = hashlib.md5(asset_path.encode("utf-8")).hexdigest()
    return asset_id, asset_name, asset_path, object_path


def _thumbnail_base_dir():
    saved_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    rel_parts = [part for part in config.THUMB_REL.strip("/\\").replace("\\", "/").split("/") if part]
    return os.path.join(saved_dir, "SceneAssembly", *rel_parts)


def thumbnail_paths(asset_id):
    filename = "{0}.png".format(asset_id)
    out_path = os.path.join(_thumbnail_base_dir(), filename)
    relative_path = "{0}/{1}".format(config.THUMB_REL.strip("/"), filename)
    thumbnail_url = _public_url(relative_path)
    return out_path, thumbnail_url


def orient_thumbnail_paths(asset_id):
    suffix = str(getattr(config, "ORIENT_THUMB_SUFFIX", "_orient") or "_orient")
    filename = "{0}{1}.png".format(asset_id, suffix)
    out_path = os.path.join(_thumbnail_base_dir(), filename)
    relative_path = "{0}/{1}".format(config.THUMB_REL.strip("/"), filename)
    thumbnail_url = _public_url(relative_path)
    return out_path, thumbnail_url


def export_thumbnail(asset_path, out_path):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    return unreal.ThumbnailLibrary.export_asset_thumbnail(asset_path, out_path, config.THUMB_RESOLUTION, _thumbnail_background_color())


def export_thumbnail_with_camera(asset_path, out_path):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    result = unreal.ThumbnailLibrary.export_asset_thumbnail_with_camera(asset_path, out_path, config.THUMB_RESOLUTION, _thumbnail_background_color())
    succeeded = bool(result.get_editor_property("success"))
    camera = _rotator_dict(result.get_editor_property("camera_rotation")) if succeeded else None
    unreal.log(
        "[SceneAssembly] Thumbnail camera export: {0} succeeded={1} camera={2}".format(
            asset_path,
            succeeded,
            camera,
        )
    )
    return succeeded, camera


def _thumbnail_background_color():
    color = getattr(config, "THUMB_BG_COLOR", (0.5, 0.5, 0.5))
    if isinstance(color, str):
        parts = [part.strip() for part in color.split(",")]
    else:
        parts = list(color)
    values = [float(parts[index]) if index < len(parts) else 0.5 for index in range(3)]
    return unreal.LinearColor(values[0], values[1], values[2], 1.0)


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


def _rotator_component(rotator, name):
    if rotator is None:
        return 0.0
    if hasattr(rotator, name):
        return float(getattr(rotator, name))
    title_name = name[:1].upper() + name[1:]
    if hasattr(rotator, title_name):
        return float(getattr(rotator, title_name))
    return 0.0


def _rotator_dict(rotator):
    if rotator is None:
        return None
    return {
        "pitch": _rotator_component(rotator, "pitch"),
        "yaw": _rotator_component(rotator, "yaw"),
        "roll": _rotator_component(rotator, "roll"),
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
        timeout=config.HTTP_TIMEOUT_SECONDS,
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
