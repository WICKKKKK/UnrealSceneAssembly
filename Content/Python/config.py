import os


BASE_URL = os.environ.get("JADE_BASE_URL", "http://localhost:8000")
API_PREFIX = os.environ.get("JADE_API_PREFIX", "/api/v1")
API_KEY = os.environ.get("JADE_API_KEY", "jade_ak_pTKyjjFlDlHt2Z1eQubm6tAttiPL0Rzn")

PROJECT_NAME = os.environ.get("JADE_PROJECT_NAME", "blockout")
CLS_COLLECTION = os.environ.get("JADE_CLS_COLLECTION", "dinov3_assets_cls_test")
PATCH_COLLECTION = os.environ.get("JADE_PATCH_COLLECTION", "dinov3_assets_patch_test")

THUMB_DIR = os.environ.get(
    "JADE_THUMB_DIR",
    r"e:\JadeProjects\JadeServices\public\thumbnails\blockout",
)
THUMB_REL = os.environ.get("JADE_THUMB_REL", "thumbnails/blockout")
THUMB_URL_PREFIX = os.environ.get("JADE_THUMB_URL_PREFIX", "/public/thumbnails/blockout")

THUMB_RESOLUTION = int(os.environ.get("JADE_THUMB_RESOLUTION", "512"))
DINO_DIMENSION = int(os.environ.get("JADE_DINO_DIMENSION", "1280"))
ASSET_PATH_PREFIXES = tuple(
    part.strip() for part in os.environ.get("JADE_ASSET_PATH_PREFIXES", "/Game").split(",") if part.strip()
)
EMBED_BATCH_SIZE = int(os.environ.get("JADE_EMBED_BATCH_SIZE", "64"))
CLS_BATCH_SIZE = int(os.environ.get("JADE_CLS_BATCH_SIZE", os.environ.get("JADE_BATCH_SIZE", "100")))
PATCH_BATCH_SIZE = int(os.environ.get("JADE_PATCH_BATCH_SIZE", "4"))
HTTP_TIMEOUT_SECONDS = float(os.environ.get("JADE_HTTP_TIMEOUT_SECONDS", "120"))
EMBED_BATCH_TIMEOUT_SECONDS = float(os.environ.get("JADE_EMBED_BATCH_TIMEOUT_SECONDS", "300"))
INGEST_TIMEOUT_SECONDS = float(os.environ.get("JADE_INGEST_TIMEOUT_SECONDS", "300"))
HTTP_MAX_RETRIES = int(os.environ.get("JADE_HTTP_MAX_RETRIES", "4"))
HTTP_RETRY_DELAY_SECONDS = float(os.environ.get("JADE_HTTP_RETRY_DELAY_SECONDS", "2"))
