#pragma once

namespace ov {

static constexpr const char* API_BASE =
    "https://orbis-vault-api.guerraf1000.workers.dev";

static constexpr const char* CATALOG_PATH = "/api/catalog";
static constexpr const char* VERSION_PATH = "/api/catalog/version";

static constexpr const char* DATA_DIR = "/data/orbis-vault";
static constexpr const char* CACHE_FILE = "/data/orbis-vault/catalog.json";
static constexpr const char* DEVICE_FILE = "/data/orbis-vault/device.json";
static constexpr const char* DOWNLOAD_DIR = "/data/orbis-vault/downloads";

static constexpr int REMOTE_POLL_SECONDS = 20;
static constexpr int HTTP_TIMEOUT_SECONDS = 30;

} // namespace ov
