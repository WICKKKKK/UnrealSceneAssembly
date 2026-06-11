import json
import time

import unreal

import asset_tagging
import config
import ingest_common as common


DATA_URI_PREFIX = "data:image/png;base64,"
VERIFY_OUTPUT_FIELDS = common.RESERVED_OUTPUT_FIELDS + ["bounding_box", "ai_tags", "ai_description"]


def _shared_fields(item):
    fields = {
        "thumbnail_url": item["thumbnail_url"],
        "asset_name": item["asset_name"],
        "asset_path": item["asset_path"],
        "asset_type": "StaticMesh",
        "public_path": item["public_path"],
    }
    if item.get("bounding_box") is not None:
        fields["bounding_box"] = item.get("bounding_box")
    if item.get("ai_tags") is not None:
        fields["ai_tags"] = item.get("ai_tags")
    if item.get("ai_description") is not None:
        fields["ai_description"] = item.get("ai_description")
    return fields


def _base_row(item):
    return {
        "project_name": config.PROJECT_NAME,
        "asset_id": item["asset_id"],
        "fields": _shared_fields(item),
    }


class Dinov3Provider(object):
    name = "dinov3"
    display_name = "DINOv3"

    def __init__(self):
        self.collection_name = config.COLLECTION_DINOv3

    def preflight(self):
        _, envelope = common.request_json("GET", "/dinov3/info")
        data = common.unwrap_data(envelope)
        if isinstance(data, dict):
            dimension = data.get("dim")
            unreal.log(
                "[SceneAssembly] DINOv3 model={0}, dim={1}, ready={2}".format(
                    data.get("model"),
                    dimension,
                    data.get("ready"),
                )
            )
            if dimension is not None and int(dimension) != config.DINO_DIMENSION:
                raise RuntimeError(
                    "Unexpected DINOv3 dim: expected {0}, got {1}".format(config.DINO_DIMENSION, dimension)
                )
        return data

    def collection_schema(self):
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
            "collection_name": self.collection_name,
            "dimension": config.DINO_DIMENSION,
            "metric_type": "IP",
            "provider": self.name,
            "category": "assets",
            "auto_id": True,
            "enable_dynamic_field": True,
            "fields": fields,
            "indexes": indexes,
            "extra": {"created_by": "SceneAssembly", "kind": "dinov3_assets"},
        }

    def reset_collection(self):
        common.reset_collection(self.collection_name, self.collection_schema())

    def validate_embed_data(self, data, context):
        if not isinstance(data, dict):
            raise RuntimeError("Unexpected {0}: {1}".format(context, data))
        if data.get("dim") != config.DINO_DIMENSION:
            raise RuntimeError("Unexpected DINOv3 dim in {0}: {1}".format(context, data.get("dim")))
        if not data.get("cls_embedding"):
            raise RuntimeError("DINOv3 response missing cls_embedding in {0}".format(context))
        if not data.get("patch_embeddings"):
            raise RuntimeError("DINOv3 response missing patch_embeddings in {0}".format(context))
        return data

    def embed_single(self, thumbnail_path):
        payload = {
            "image": {"base64": common.encode_thumbnail_base64(thumbnail_path)},
            "return_patches": True,
            "l2_normalize": True,
        }
        _, envelope = common.request_json("POST", "/dinov3/embed", payload, timeout=config.HTTP_TIMEOUT_SECONDS)
        data = common.unwrap_data(envelope)
        return self.validate_embed_data(data, "DINOv3 embed response")

    def embed_batch(self, thumbnail_paths):
        if not thumbnail_paths:
            return []

        payload = {
            "images": [{"base64": common.encode_thumbnail_base64(path)} for path in thumbnail_paths],
            "return_patches": True,
            "l2_normalize": True,
        }
        _, envelope = common.request_json(
            "POST",
            "/dinov3/embed/batch",
            payload,
            timeout=config.EMBED_BATCH_TIMEOUT_SECONDS,
        )
        data = common.unwrap_data(envelope)
        if not isinstance(data, dict):
            raise RuntimeError("Unexpected DINOv3 batch response: {0}".format(data))
        results = data.get("results")
        if not isinstance(results, list):
            raise RuntimeError("DINOv3 batch response missing results")
        if len(results) != len(thumbnail_paths):
            raise RuntimeError(
                "DINOv3 batch result count mismatch: expected {0}, got {1}".format(
                    len(thumbnail_paths),
                    len(results),
                )
            )
        return [self.validate_embed_data(result, "DINOv3 batch result[{0}]".format(index)) for index, result in enumerate(results)]

    def build_patch_structs(self, embed_data):
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

    def build_row(self, item, embed_data):
        row = _base_row(item)
        row["cls_embedding"] = embed_data["cls_embedding"]
        row["patches"] = self.build_patch_structs(embed_data)
        return row


class ClipProvider(object):
    name = "clip"
    display_name = "CLIP"

    def __init__(self):
        self.collection_name = config.COLLECTION_CLIP

    def preflight(self):
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

    def collection_schema(self):
        return {
            "collection_name": self.collection_name,
            "dimension": config.CLIP_DIMENSION,
            "metric_type": "COSINE",
            "provider": self.name,
            "category": "assets",
            "enable_dynamic_field": True,
            "extra": {"created_by": "SceneAssembly", "kind": "clip_assets"},
        }

    def reset_collection(self):
        common.reset_collection(self.collection_name, self.collection_schema())

    def _thumbnail_data_uri(self, thumbnail_path):
        return DATA_URI_PREFIX + common.encode_thumbnail_base64(thumbnail_path)

    def validate_embedding_result(self, result, context):
        if not isinstance(result, dict):
            raise RuntimeError("Unexpected {0}: {1}".format(context, result))
        if result.get("dimension") != config.CLIP_DIMENSION:
            raise RuntimeError("Unexpected CLIP dimension in {0}: {1}".format(context, result.get("dimension")))
        vector = result.get("vector")
        if not isinstance(vector, list) or len(vector) != config.CLIP_DIMENSION:
            raise RuntimeError("CLIP response has invalid vector in {0}".format(context))
        return vector

    def _parse_embedding_results(self, data, expected_count):
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
            vectors[index] = self.validate_embedding_result(result, "CLIP embed result[{0}]".format(index))

        missing = [index for index, vector in enumerate(vectors) if vector is None]
        if missing:
            raise RuntimeError("CLIP embed response missing indexes: {0}".format(missing))
        return vectors

    def embed_single(self, thumbnail_path):
        return self.embed_batch([thumbnail_path])[0]

    def embed_batch(self, thumbnail_paths):
        if not thumbnail_paths:
            return []

        payload = {"images": [{"url": self._thumbnail_data_uri(path)} for path in thumbnail_paths]}
        _, envelope = common.request_json(
            "POST",
            "/clip/embed/image",
            payload,
            timeout=getattr(config, "EMBED_BATCH_TIMEOUT_SECONDS", config.HTTP_TIMEOUT_SECONDS),
        )
        data = common.unwrap_data(envelope)
        return self._parse_embedding_results(data, len(thumbnail_paths))

    def build_row(self, item, vector):
        row = _base_row(item)
        row["vector"] = vector
        return row


def _providers_for_mode(mode):
    normalized = str(mode or "all").strip().lower()
    if normalized == "all":
        return normalized, [Dinov3Provider(), ClipProvider()]
    if normalized == "dinov3":
        return normalized, [Dinov3Provider()]
    if normalized == "clip":
        return normalized, [ClipProvider()]
    raise ValueError("Unsupported ingest mode: {0}. Expected one of: all, clip, dinov3".format(mode))


def _provider_for_name(provider):
    normalized = str(provider or "dinov3").strip().lower()
    if normalized == "dinov3":
        return Dinov3Provider()
    if normalized == "clip":
        return ClipProvider()
    raise ValueError("Unsupported provider: {0}. Expected one of: clip, dinov3".format(provider))


def _record_prepare_failure(shared_failed, asset_path, exc):
    common.record_failed_asset(shared_failed, asset_path, exc)


def _combined_failed(shared_failed, provider_failed):
    return list(shared_failed) + list(provider_failed)


def enrich_ai_metadata(items):
    if not items or not getattr(config, "TAGGING_ENABLED", False):
        return

    unreal.log("[SceneAssembly] AI tagging batch: {0} thumbnails".format(len(items)))
    try:
        results = asset_tagging.generate_batch(items, getattr(config, "TAGGING_CONCURRENCY", 6))
    except Exception as exc:
        unreal.log_warning("[SceneAssembly] AI tagging batch failed: {0}".format(exc))
        results = {}

    for item in items:
        result = results.get(item.get("asset_id")) or {}
        item["ai_tags"] = result.get("ai_tags") or []
        item["ai_description"] = result.get("ai_description") or ""


def _add_provider_row(provider, item, embed_result, rows):
    rows.append(provider.build_row(item, embed_result))


def _flush_provider_embeddings(provider, pending, rows, failed):
    if not pending:
        return 0

    unreal.log("[SceneAssembly] {0} embedding batch: {1} thumbnails".format(provider.display_name, len(pending)))
    succeeded = 0
    try:
        embed_results = provider.embed_batch([item["out_path"] for item in pending])
    except Exception as exc:
        unreal.log_warning(
            "[SceneAssembly] {0} batch embedding failed, falling back to single-image embedding: {1}".format(
                provider.display_name,
                exc,
            )
        )
        for item in pending:
            try:
                embed_result = provider.embed_single(item["out_path"])
                _add_provider_row(provider, item, embed_result, rows)
                succeeded += 1
            except Exception as item_exc:
                common.record_failed_asset(failed, item["asset_path"], item_exc)
        return succeeded

    for item, embed_result in zip(pending, embed_results):
        try:
            _add_provider_row(provider, item, embed_result, rows)
            succeeded += 1
        except Exception as exc:
            common.record_failed_asset(failed, item["asset_path"], exc)
    return succeeded


def _process_pending_batch(pending, providers, rows_by_provider, failed_by_provider, succeeded_by_provider):
    if not pending:
        return

    enrich_ai_metadata(pending)
    for provider in providers:
        rows = rows_by_provider[provider.name]
        failed = failed_by_provider[provider.name]
        succeeded_by_provider[provider.name] += _flush_provider_embeddings(provider, pending, rows, failed)
        common.flush_batches(rows, provider.collection_name, failed=failed)
    pending[:] = []


def _prepare_asset_item(asset_data, index, total):
    asset_id, asset_name, asset_path = common.asset_identity(asset_data)
    out_path, public_path, thumbnail_url = common.thumbnail_paths(asset_id)
    unreal.log("[SceneAssembly] [{0}/{1}] {2}".format(index, total, asset_path))

    bounding_box = common.asset_bounding_box(asset_data, asset_path)
    if not common.export_thumbnail(asset_path, out_path):
        raise RuntimeError("thumbnail export returned false")

    return {
        "asset_id": asset_id,
        "asset_name": asset_name,
        "asset_path": asset_path,
        "out_path": out_path,
        "thumbnail_url": thumbnail_url,
        "public_path": public_path,
        "bounding_box": bounding_box,
    }


def ingest_static_meshes(mode="all"):
    started_at = time.time()
    normalized_mode, providers = _providers_for_mode(mode)
    provider_names = [provider.name for provider in providers]

    common.auth_self_check()
    for provider in providers:
        provider.preflight()
    for provider in providers:
        provider.reset_collection()

    asset_data_list = common.enumerate_static_mesh_assets()
    total = len(asset_data_list)
    unreal.log(
        "[SceneAssembly] Found {0} StaticMesh assets for {1} ingest.".format(
            total,
            ", ".join(provider_names),
        )
    )

    rows_by_provider = {provider.name: [] for provider in providers}
    failed_by_provider = {provider.name: [] for provider in providers}
    succeeded_by_provider = {provider.name: 0 for provider in providers}
    shared_failed = []
    pending = []
    embed_batch_size = max(1, int(getattr(config, "EMBED_BATCH_SIZE", 1)))

    for index, asset_data in enumerate(asset_data_list, start=1):
        asset_path = "<unknown>"
        try:
            asset_path = common.soft_object_path_string(asset_data)
            pending.append(_prepare_asset_item(asset_data, index, total))
        except Exception as exc:
            _record_prepare_failure(shared_failed, asset_path, exc)
            continue

        if len(pending) >= embed_batch_size:
            _process_pending_batch(pending, providers, rows_by_provider, failed_by_provider, succeeded_by_provider)

    _process_pending_batch(pending, providers, rows_by_provider, failed_by_provider, succeeded_by_provider)
    for provider in providers:
        common.flush_batches(
            rows_by_provider[provider.name],
            provider.collection_name,
            force=True,
            failed=failed_by_provider[provider.name],
        )

    elapsed = time.time() - started_at
    provider_summaries = {}
    for provider in providers:
        provider_failed = _combined_failed(shared_failed, failed_by_provider[provider.name])
        provider_summaries[provider.name] = {
            "collection": provider.collection_name,
            "total": total,
            "succeeded": succeeded_by_provider[provider.name],
            "failed": provider_failed,
            "elapsed_seconds": elapsed,
        }
        unreal.log(
            "[SceneAssembly] {0} ingest finished. total={1}, succeeded={2}, failed={3}, elapsed={4:.1f}s".format(
                provider.display_name,
                total,
                succeeded_by_provider[provider.name],
                len(provider_failed),
                elapsed,
            )
        )
        for item in provider_failed:
            unreal.log_warning("[SceneAssembly] {0} failed: {1} | {2}".format(provider.display_name, item["asset_path"], item["error"]))

    return {
        "mode": normalized_mode,
        "total": total,
        "providers": provider_summaries,
        "shared_failed": shared_failed,
        "elapsed_seconds": elapsed,
    }


def ingest_all():
    return ingest_static_meshes("all")


def ingest_clip():
    return ingest_static_meshes("clip")


def ingest_dinov3():
    return ingest_static_meshes("dinov3")


def verify_query(provider="dinov3", limit=10):
    if isinstance(provider, int):
        limit = provider
        provider = "dinov3"
    selected = _provider_for_name(provider)
    payload = {
        "filters": {"project_names": [config.PROJECT_NAME]},
        "output_fields": VERIFY_OUTPUT_FIELDS,
        "limit": limit,
        "offset": 0,
    }
    _, envelope = common.request_json("POST", "/milvus/collections/{0}/query_assets".format(selected.collection_name), payload)
    data = common.unwrap_data(envelope)
    unreal.log(
        "[SceneAssembly] {0} query verification: {1}".format(
            selected.display_name,
            json.dumps(data, ensure_ascii=False)[:2000],
        )
    )
    return data


def verify_search_text(text, limit=5):
    provider = ClipProvider()
    payload = {
        "text": text,
        "limit": limit,
        "filters": {"project_names": [config.PROJECT_NAME]},
        "output_fields": VERIFY_OUTPUT_FIELDS,
    }
    _, envelope = common.request_json(
        "POST",
        "/clip/collections/{0}/search/single_text".format(provider.collection_name),
        payload,
        timeout=getattr(config, "HTTP_TIMEOUT_SECONDS", 120.0),
    )
    data = common.unwrap_data(envelope)
    unreal.log("[SceneAssembly] CLIP text search verification: {0}".format(json.dumps(data, ensure_ascii=False)[:2000]))
    return data


if __name__ == "__main__":
    ingest_static_meshes()
