# 后端服务接入参数（直接配置，避免与本机环境变量冲突）
BASE_URL = "http://localhost:8000"
API_PREFIX = "/api/v1"
API_KEY = "jade_ak_pTKyjjFlDlHt2Z1eQubm6tAttiPL0Rzn"

# 项目与 Milvus 集合名
PROJECT_NAME = "blockout"
COLLECTION = "dinov3_assets_test"
COLLECTION_CLIP = "clip_assets_test"

# 缩略图本地输出目录与对外访问路径
THUMB_DIR = r"e:\JadeProjects\JadeServices\public\thumbnails\blockout"
THUMB_REL = "thumbnails/blockout"
THUMB_URL_PREFIX = "/public/thumbnails/blockout"

# 缩略图与模型维度
THUMB_RESOLUTION = 512
DINO_DIMENSION = 1280
CLIP_DIMENSION = 768

# 仅处理这些资源路径前缀下的资产
ASSET_PATH_PREFIXES = ("/Game/PolygonStreetRacer",)

# 批处理大小
EMBED_BATCH_SIZE = 64
INGEST_BATCH_SIZE = 4

# AI asset tagging
TAGGING_ENABLED = False
TAGGING_MODEL = "timiai/gemini-3.5-flash"
TAGGING_CONCURRENCY = 16
TAGGING_MAX_RETRY = 3
TAGGING_TIMEOUT_SECONDS = 120.0
TAGGING_MAX_TOKENS = 512

# 超时与重试
HTTP_TIMEOUT_SECONDS = 120.0
EMBED_BATCH_TIMEOUT_SECONDS = 300.0
INGEST_TIMEOUT_SECONDS = 300.0
HTTP_MAX_RETRIES = 4
HTTP_RETRY_DELAY_SECONDS = 2.0
