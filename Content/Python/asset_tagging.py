import ast
import concurrent.futures
import json
import re
import time
import urllib.error
import urllib.request

import unreal

import config
import ingest_static_meshes as common


DATA_URI_PREFIX = "data:image/png;base64,"


class TaggingApiError(RuntimeError):
    def __init__(self, method, url, status, body):
        super().__init__("{0} {1} failed with HTTP {2}: {3}".format(method, url, status, body))
        self.method = method
        self.url = url
        self.status = status
        self.body = body


def _empty_result(error=None):
    result = {"ai_tags": [], "ai_description": ""}
    if error:
        result["error"] = str(error)
    return result


def _float_or_none(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def _size_meters_from_item(item):
    bounding_box = item.get("bounding_box") or {}
    if not isinstance(bounding_box, dict):
        return None

    bbox_size = bounding_box.get("BboxSize") or {}
    if not isinstance(bbox_size, dict):
        return None

    x = _float_or_none(bbox_size.get("X"))
    y = _float_or_none(bbox_size.get("Y"))
    z = _float_or_none(bbox_size.get("Z"))
    if x is None or y is None or z is None:
        return None
    if x < 0.0 or y < 0.0 or z < 0.0:
        return None

    return (x * 0.01, y * 0.01, z * 0.01)


def _build_prompt(item):
    asset_name = item.get("asset_name") or ""
    asset_path = item.get("asset_path") or ""
    asset_type = "StaticMesh"
    size_meters = _size_meters_from_item(item)
    size_prompt = ""
    if size_meters is not None:
        size_prompt = "\n- Size: ({0:.3f}m, {1:.3f}m, {2:.3f}m)".format(
            size_meters[0],
            size_meters[1],
            size_meters[2],
        )

    example_json = {
        "description": "A damaged cyan marble rock with an irregular natural shape.",
        "tags": ["marble", "rock", "cyan", "damaged", "irregular shape"],
    }
    return (
        "You are an expert game-asset visual tagging assistant. The image is a thumbnail of one asset.\n"
        "Use both the image and the metadata below to identify the asset subject and its key visual features.\n\n"
        "Asset metadata:\n"
        "- Name: {0}\n"
        "- Type: {1}\n"
        "- Path: {2}{3}\n\n"
        "Output strict JSON only, with exactly these keys: description and tags.\n"
        "Schema example: {4}\n\n"
        "Rules:\n"
        "- The description and all tags must be in English.\n"
        "- The description should briefly describe only the asset itself. Do not describe lighting, environment, "
        "rendering, camera angle, thumbnail quality, or that it is a 3D model.\n"
        "- Tags must be deterministic, holistic, and representative of the asset's important visual semantics.\n"
        "- Use 5 to 12 concise tags. Prefer lowercase nouns or short adjective-noun phrases.\n"
        "- Do not include special symbols in tags.\n"
        "- Return JSON only, no Markdown, no code fences, no extra commentary."
    ).format(asset_name, asset_type, asset_path, size_prompt, json.dumps(example_json, ensure_ascii=False))


def _thumbnail_data_uri(thumbnail_path):
    return DATA_URI_PREFIX + common.encode_thumbnail_base64(thumbnail_path)


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


def _request_json(method, path, payload=None, timeout=None):
    url = _api_url(path)
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(url, data=body, headers=_headers(), method=method)
    try:
        with urllib.request.urlopen(request, timeout=timeout or config.HTTP_TIMEOUT_SECONDS) as response:
            return response.status, _decode_json(response.read())
    except urllib.error.HTTPError as exc:
        response_body = exc.read().decode("utf-8", errors="replace")
        raise TaggingApiError(method, url, exc.code, response_body)
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError("{0} {1} failed: {2}".format(method, url, exc))


def _message_content_text(content):
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts = []
        for part in content:
            if isinstance(part, str):
                parts.append(part)
            elif isinstance(part, dict) and isinstance(part.get("text"), str):
                parts.append(part.get("text"))
        return "\n".join(parts)
    return ""


def _chat_completion_text(messages):
    payload = {
        "model": getattr(config, "TAGGING_MODEL", "timiai/gemini-3.5-flash"),
        "messages": messages,
        "temperature": 0,
        "max_tokens": int(getattr(config, "TAGGING_MAX_TOKENS", 512)),
    }
    _, response = _request_json(
        "POST",
        "/chat/completions",
        payload,
        timeout=getattr(config, "TAGGING_TIMEOUT_SECONDS", config.HTTP_TIMEOUT_SECONDS),
    )
    if not isinstance(response, dict):
        raise RuntimeError("Unexpected chat completion response: {0}".format(response))

    choices = response.get("choices")
    if not isinstance(choices, list) or not choices:
        raise RuntimeError("Chat completion response missing choices")

    choice = choices[0]
    if not isinstance(choice, dict):
        raise RuntimeError("Unexpected chat completion choice: {0}".format(choice))

    message = choice.get("message") or {}
    if isinstance(message, dict):
        content = _message_content_text(message.get("content"))
    else:
        content = ""
    if not content and isinstance(choice.get("text"), str):
        content = choice.get("text")
    if not content:
        raise RuntimeError("Chat completion response has empty content")
    return content


def _decode_jsonish(text):
    cleaned = (text or "").strip()
    if not cleaned:
        raise ValueError("empty tagging response")

    try:
        return json.loads(cleaned)
    except ValueError:
        pass

    decoder = json.JSONDecoder()
    for index, char in enumerate(cleaned):
        if char not in "[{":
            continue
        try:
            value, _ = decoder.raw_decode(cleaned[index:])
            return value
        except ValueError:
            continue

    start = cleaned.find("{")
    end = cleaned.rfind("}")
    if start >= 0 and end > start:
        return ast.literal_eval(cleaned[start : end + 1])

    start = cleaned.find("[")
    end = cleaned.rfind("]")
    if start >= 0 and end > start:
        return ast.literal_eval(cleaned[start : end + 1])

    raise ValueError("tagging response does not contain JSON")


def _split_tags_string(value):
    return [part.strip() for part in re.split(r"[,;\uFF0C\uFF1B]", value) if part.strip()]


def _normalize_tags(value):
    if isinstance(value, str):
        value = _split_tags_string(value)
    if not isinstance(value, list):
        return []

    tags = []
    seen = set()
    for item in value:
        tag = str(item).strip()
        if not tag:
            continue
        tag = re.sub(r"\s+", " ", tag)
        key = tag.lower()
        if key in seen:
            continue
        seen.add(key)
        tags.append(tag)
    return tags


def _description_from_value(value):
    if value is None:
        return ""
    if isinstance(value, str):
        return value.strip()
    return str(value).strip()


def _parse_result(text):
    value = _decode_jsonish(text)
    if isinstance(value, list):
        return _normalize_tags(value), ""
    if not isinstance(value, dict):
        return [], ""

    tags_value = value.get("tags")
    if tags_value is None:
        tags_value = value.get("ai_tags")
    if tags_value is None:
        tags_value = value.get("\u6807\u7b7e")

    description_value = value.get("description")
    if description_value is None:
        description_value = value.get("ai_description")
    if description_value is None:
        description_value = value.get("\u56fe\u7247\u63cf\u8ff0")

    return _normalize_tags(tags_value), _description_from_value(description_value)


def _generate_for_item_once(item):
    messages = [
        {
            "role": "user",
            "content": [
                {"type": "text", "text": _build_prompt(item)},
                {"type": "image_url", "image_url": {"url": _thumbnail_data_uri(item["out_path"])}},
            ],
        }
    ]
    text = _chat_completion_text(messages)
    tags, description = _parse_result(text)
    if not tags:
        raise RuntimeError("Tagging response did not contain tags: {0}".format(text[:500]))
    return {"ai_tags": tags, "ai_description": description}


def generate_for_item(item):
    max_retry = max(1, int(getattr(config, "TAGGING_MAX_RETRY", 3)))
    retry_delay = float(getattr(config, "HTTP_RETRY_DELAY_SECONDS", 1.0))
    last_error = None
    for attempt in range(1, max_retry + 1):
        try:
            return _generate_for_item_once(item)
        except Exception as exc:
            last_error = exc
            if attempt < max_retry:
                time.sleep(retry_delay * attempt)
    return _empty_result(last_error)


def generate_batch(items, concurrency=None):
    if not items:
        return {}

    worker_count = concurrency
    if worker_count is None:
        worker_count = getattr(config, "TAGGING_CONCURRENCY", 6)
    worker_count = max(1, min(len(items), int(worker_count)))

    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=worker_count) as executor:
        future_to_item = {executor.submit(generate_for_item, item): item for item in items}
        for future in concurrent.futures.as_completed(future_to_item):
            item = future_to_item[future]
            asset_id = item.get("asset_id")
            try:
                result = future.result()
            except Exception as exc:
                result = _empty_result(exc)

            if result.get("error"):
                unreal.log_warning(
                    "[SceneAssembly] AI tagging failed for {0}: {1}".format(
                        item.get("asset_path") or asset_id,
                        result.get("error"),
                    )
                )
            results[asset_id] = {
                "ai_tags": result.get("ai_tags") or [],
                "ai_description": result.get("ai_description") or "",
            }
    return results
