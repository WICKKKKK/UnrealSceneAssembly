# UnrealSceneAssembly

Editor-only UE plugin for exporting StaticMesh thumbnails and ingesting DINOv3 vectors into Jade AI Services.

## What It Does

- Enumerates every `StaticMesh` asset in the project.
- Exports a PNG thumbnail with the asset thumbnail renderer.
- Saves thumbnails to `e:\JadeProjects\JadeServices\public\thumbnails\blockout` as `<md5(asset_path)>.png`.
- Calls `POST /api/v1/dinov3/embed` with `public_path`.
- Recreates and fills two Milvus collections:
  - `dinov3_assets_cls_test` for `cls_embedding`.
  - `dinov3_assets_patch_test` for `patches[embedding]`.

## Manual Run

1. Start the Jade backend at `http://localhost:8000` and make sure DINOv3 and Milvus are available.
2. Open the UE project and let Unreal rebuild this plugin if prompted.
3. Open the Unreal Python console.
4. Run:

```python
import importlib
import ingest_static_meshes
importlib.reload(ingest_static_meshes)
ingest_static_meshes.ingest_static_meshes()
```

## Quick Verification

After ingestion finishes, run:

```python
ingest_static_meshes.verify_query(10)
```

Search verification can also be done through the backend endpoints:

- `POST /api/v1/dinov3/collections/dinov3_assets_cls_test/search/global`
- `POST /api/v1/dinov3/collections/dinov3_assets_patch_test/search/roi`
- `POST /api/v1/dinov3/collections/dinov3_assets_patch_test/search/rerank`

Use `output_fields` such as `thumbnail_url`, `asset_name`, `asset_path`, `asset_type`, and `public_path`.

## Config

Defaults live in `Content/Python/config.py`. Environment variables can override them:

- `JADE_BASE_URL`
- `JADE_API_PREFIX`
- `JADE_API_KEY`
- `JADE_PROJECT_NAME`
- `JADE_CLS_COLLECTION`
- `JADE_PATCH_COLLECTION`
- `JADE_THUMB_DIR`
- `JADE_THUMB_REL`
- `JADE_THUMB_URL_PREFIX`
- `JADE_THUMB_RESOLUTION`
- `JADE_CLS_BATCH_SIZE`
- `JADE_PATCH_BATCH_SIZE`
- `JADE_HTTP_TIMEOUT_SECONDS`

## Notes

- Each run deletes and recreates both configured collections. A `404` during delete is treated as success.
- `thumbnail_url` is stored as `/public/thumbnails/blockout/<md5>.png`.
- DINOv3 embedding uses `public_path` as `thumbnails/blockout/<md5>.png`.
- Patch rows are uploaded in small batches by default because each row contains all patch vectors.
