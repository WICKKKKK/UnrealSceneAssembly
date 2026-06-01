# UnrealSceneAssembly

仅在编辑器下使用的 UE 插件：导出 StaticMesh 的缩略图，并将 DINOv3 向量入库到 Jade AI Services。

## 功能说明

- 枚举工程中所有 `StaticMesh` 资产。
- 通过资产缩略图渲染器导出 PNG 缩略图。
- 缩略图保存到 `e:\JadeProjects\JadeServices\public\thumbnails\blockout`，文件名为 `<md5(asset_path)>.png`。
- 使用 `public_path` 调用 `POST /api/v1/dinov3/embed` 接口生成向量。
- 重建并写入两个 Milvus 集合：
  - `dinov3_assets_cls_test`，用于存放 `cls_embedding`。
  - `dinov3_assets_patch_test`，用于存放 `patches[embedding]`。

## 手动运行

1. 启动 Jade 后端 `http://localhost:8000`，确保 DINOv3 与 Milvus 服务可用。
2. 打开 UE 工程，按提示让 Unreal 重新编译该插件。
3. 打开 Unreal Python 控制台。
4. 执行：

```python
import importlib
import ingest_static_meshes
importlib.reload(ingest_static_meshes)
ingest_static_meshes.ingest_static_meshes()
```

## 快速校验

入库结束后执行：

```python
ingest_static_meshes.verify_query(10)
```

也可以通过后端接口校验检索：

- `POST /api/v1/dinov3/collections/dinov3_assets_cls_test/search/global`
- `POST /api/v1/dinov3/collections/dinov3_assets_patch_test/search/roi`
- `POST /api/v1/dinov3/collections/dinov3_assets_patch_test/search/rerank`

`output_fields` 可使用：`thumbnail_url`、`asset_name`、`asset_path`、`asset_type`、`public_path`。

## 配置

所有运行参数直接定义在 `Content/Python/config.py` 中，按需修改即可，无需配置环境变量。主要字段如下：

- `BASE_URL` / `API_PREFIX` / `API_KEY`：后端基础地址与接入凭证。
- `PROJECT_NAME`：项目名（用作 Milvus 分区键）。
- `CLS_COLLECTION` / `PATCH_COLLECTION`：两个 Milvus 集合名。
- `THUMB_DIR` / `THUMB_REL` / `THUMB_URL_PREFIX`：缩略图本地保存目录与对外访问路径。
- `THUMB_RESOLUTION`：缩略图边长（像素）。
- `DINO_DIMENSION`：DINOv3 向量维度。
- `ASSET_PATH_PREFIXES`：仅处理这些路径前缀下的资产，默认 `("/Game",)`。
- `EMBED_BATCH_SIZE` / `CLS_BATCH_SIZE` / `PATCH_BATCH_SIZE`：批处理大小。
- `HTTP_TIMEOUT_SECONDS` / `EMBED_BATCH_TIMEOUT_SECONDS` / `INGEST_TIMEOUT_SECONDS`：超时时间。
- `HTTP_MAX_RETRIES` / `HTTP_RETRY_DELAY_SECONDS`：重试次数与重试间隔。

## 注意事项

- 每次运行都会先删除并重建配置中的两个集合，删除时返回 `404` 视为成功。
- `thumbnail_url` 存为 `/public/thumbnails/blockout/<md5>.png`。
- DINOv3 计算向量时使用 `public_path`，格式为 `thumbnails/blockout/<md5>.png`。
- Patch 行因为单行包含全部 patch 向量，默认会以较小的批次上传。
