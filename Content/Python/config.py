# 后端服务接入参数（直接配置，避免与本机环境变量冲突）
BASE_URL = "http://localhost:8000"
API_PREFIX = "/api/v1"
API_KEY = "jade_ak_pTKyjjFlDlHt2Z1eQubm6tAttiPL0Rzn"

# 项目与 Milvus 集合名
PROJECT_NAME = "blockout"
COLLECTION_DINOv3 = "dinov3_assets_test"
COLLECTION_CLIP = "clip_assets_test"

# 缩略图本地输出目录与对外访问路径
THUMB_DIR = r"e:\UE_Projects\blank_ue551\Saved\SceneAssembly\thumbnails\blockout"
THUMB_REL = "thumbnails/blockout"
ORIENT_THUMB_SUFFIX = "_orient"

# 缩略图与模型维度
THUMB_RESOLUTION = 512
THUMB_BG_COLOR = (0.0, 0.0, 0.0)
DINO_DIMENSION = 1280
CLIP_DIMENSION = 768

# 仅处理这些资源路径前缀下的资产
ASSET_PATH_PREFIXES = ("/Game/FC_MilitaryCamp",)

# 批处理大小
EMBED_BATCH_SIZE = 64
ORIENT_ENABLED = True
ORIENT_PREPROCESS_BATCH_SIZE = 64
ORIENT_PREDICT_BATCH_SIZE = 1
INGEST_BATCH_SIZE = 64

# AI asset tagging
TAGGING_ENABLED = False
TAGGING_MODEL = "timiai/gemini-3.5-flash"
TAGGING_CONCURRENCY = 16
TAGGING_MAX_RETRY = 3
TAGGING_MAX_TOKENS = 512

# 超时与重试
HTTP_TIMEOUT_SECONDS = 600.0
HTTP_MAX_RETRIES = 4
HTTP_RETRY_DELAY_SECONDS = 2.0
