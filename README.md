# UnrealSceneAssembly

仅在编辑器下使用的 UE 插件：导出 StaticMesh 的缩略图，并将 DINOv3 向量入库到 Jade AI Services。

## 功能说明

- 枚举工程中所有 `StaticMesh` 资产。
- 通过资产缩略图渲染器导出 PNG 缩略图。
- 缩略图保存到 `e:\JadeProjects\JadeServices\public\thumbnails\blockout`，文件名为 `<md5(asset_path)>.png`。
- 使用 `public_path` 调用 `POST /api/v1/dinov3/embed` 接口生成向量。
- 重建并写入 `dinov3_assets_test` Milvus 集合，集合内同时保存 `cls_embedding` 与 patch embeddings。
- 提供编辑器内 MCP 管理面板，可检测环境、一键安装依赖、检查端口并启停本地 MCP 服务。

## MCP Agent 接入

本插件启动时会自动执行 `Content/Python/init_unreal.py` 并初始化 MCP 控制器，但默认不会启动 MCP 服务。打开编辑器后通过主工具栏 `MCP` 按钮，或菜单 `Window > Scene Assembly MCP` 打开管理面板，再手动启动。

管理面板启动时会同时启动两层本地服务：

- `127.0.0.1:8766`：UE 进程内 JSON-line TCP bridge，只使用标准库，负责把请求转到 Unreal 主线程执行。
- `http://127.0.0.1:8780/mcp`：外部 FastMCP streamable-http server，供 opencode、Claude Code 等 Agent 连接。

当前 v1 暴露两个 MCP 工具：

- `unreal_ping`：检查 Agent -> MCP server -> UE bridge 的连通性。
- `unreal_get_project_info`：返回当前 Unreal 工程、引擎、Python 与 MCP 配置信息。

### 管理面板

面板提供以下操作：

- `Check Environment`：检测插件本地 Python 依赖是否已安装。
- `Install Dependencies`：使用 UE 自带 Python 或 `UESA_MCP_PYTHON` 指定的 Python，将依赖安装到 `Content/Python/mcp_server/site-packages`。
- `Check Port`：启动前检测当前 MCP HTTP 端口是否可绑定。
- `Start MCP`：仅在依赖已安装且端口可用时启用，同时启动 UE bridge 和外部 MCP server。
- `Stop MCP`：停止外部 MCP server 并关闭 UE bridge。

如果在面板中修改端口，需要手动同步更新 Agent 配置，例如 `.opencode/opencode.json` 中的 MCP URL。

### 安装 MCP 依赖

FastMCP 依赖不会安装到 Unreal Engine 的全局 Python site-packages，而是安装到插件本地目录：

```powershell
python -m pip install -r "Content\Python\mcp_server\requirements.txt" --target "Content\Python\mcp_server\site-packages"
```

也可以使用 UE 自带 Python，例如：

```powershell
"<UE_ROOT>\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" -m pip install -r "Content\Python\mcp_server\requirements.txt" --target "Content\Python\mcp_server\site-packages"
```

推荐优先使用管理面板中的 `Install Dependencies`。如果需要离线或手动安装，也可以执行上述命令。依赖未安装时，面板会禁用 `Start MCP`。

### Agent 配置

将 Agent 的 MCP server 地址配置为：

```text
http://127.0.0.1:8780/mcp
```

如果需要修改默认端口或在无人值守场景中自动启动，可在启动 UE 前设置环境变量：

```powershell
$env:UESA_MCP_HOST = "127.0.0.1"
$env:UESA_MCP_PORT = "8780"
$env:UESA_MCP_PATH = "/mcp"
$env:UESA_BRIDGE_HOST = "127.0.0.1"
$env:UESA_BRIDGE_PORT = "8766"
$env:UESA_MCP_AUTO_START = "1"
```

`UESA_MCP_AUTO_START` 默认关闭。设为 `1` 时会在 UE 启动时自动启动 bridge 与外部 MCP server，主要用于 CI 或无人值守场景。

其他可选项：

- `UESA_MCP_REQUEST_TIMEOUT`：bridge 请求超时秒数，默认 `30`。
- `UESA_MCP_VERBOSE`：设为 `1` 时输出 bridge traceback。
- `UESA_MCP_PYTHON`：指定外部 MCP server 使用的 Python 可执行文件。
- `UESA_MCP_ALLOW_GLOBAL_DEPS`：设为 `1` 时允许启动时使用外部 Python 全局依赖，不强制要求插件本地 `site-packages`。

### 手动自测

在面板中启动 MCP 后，可在 Unreal Python 控制台直接测试 bridge：

```python
from mcp_server.bridge_client import call_bridge
call_bridge("ping")
call_bridge("get_project_info")
```

也可以通过已连接的 Agent 调用 MCP 工具 `unreal_ping` 与 `unreal_get_project_info`。

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

- `POST /api/v1/milvus/collections/dinov3_assets_test/query_assets`

`output_fields` 可使用：`thumbnail_url`、`asset_name`、`asset_path`、`asset_type`、`public_path`。

## 配置

所有运行参数直接定义在 `Content/Python/config.py` 中，按需修改即可，无需配置环境变量。主要字段如下：

- `BASE_URL` / `API_PREFIX` / `API_KEY`：后端基础地址与接入凭证。
- `PROJECT_NAME`：项目名（用作 Milvus 分区键）。
- `COLLECTION`：Milvus 集合名。
- `THUMB_DIR` / `THUMB_REL` / `THUMB_URL_PREFIX`：缩略图本地保存目录与对外访问路径。
- `THUMB_RESOLUTION`：缩略图边长（像素）。
- `DINO_DIMENSION`：DINOv3 向量维度。
- `ASSET_PATH_PREFIXES`：仅处理这些路径前缀下的资产，默认 `("/Game",)`。
- `EMBED_BATCH_SIZE` / `INGEST_BATCH_SIZE`：批处理大小。
- `HTTP_TIMEOUT_SECONDS` / `EMBED_BATCH_TIMEOUT_SECONDS` / `INGEST_TIMEOUT_SECONDS`：超时时间。
- `HTTP_MAX_RETRIES` / `HTTP_RETRY_DELAY_SECONDS`：重试次数与重试间隔。

## 注意事项

- 每次运行都会先删除并重建配置中的集合，删除时返回 `404` 视为成功。
- `thumbnail_url` 存为 `/public/thumbnails/blockout/<md5>.png`。
- DINOv3 计算向量时使用 `public_path`，格式为 `thumbnails/blockout/<md5>.png`。
- Patch embeddings 与 cls embedding 写入同一个资产行，默认会以较小的批次上传。
