#include "catalog_client.hpp"
#include "config.hpp"
#include <stdio.h>
#include <stdlib.h>

namespace ov {

CatalogClient::CatalogClient(HttpClient& http) : http_(http) {}

static bool read_all(const char* path, std::string& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n < 0) { fclose(f); return false; }
    out.resize(static_cast<size_t>(n));
    if (n > 0) fread(&out[0], 1, static_cast<size_t>(n), f);
    fclose(f);
    return true;
}

static bool write_all(const char* path, const std::string& data) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return true;
}

bool CatalogClient::parseRevision(const std::string& json, int& revision) {
    const std::string key = "\"revision\"";
    auto p = json.find(key);
    if (p == std::string::npos) return false;
    p = json.find(':', p + key.size());
    if (p == std::string::npos) return false;
    revision = atoi(json.c_str() + p + 1);
    return revision >= 0;
}

bool CatalogClient::fetchRevision(int& revision, std::string& error) {
    auto r = http_.get(std::string(API_BASE) + VERSION_PATH);
    if (!r.ok()) {
        error = r.error.empty() ? ("HTTP " + std::to_string(r.status)) : r.error;
        return false;
    }
    if (!parseRevision(r.body, revision)) {
        error = "invalid version payload";
        return false;
    }
    return true;
}

bool CatalogClient::parseCatalog(const std::string& json, Catalog& catalog,
                                 std::string& error) {
    // v0.1: revision is parsed now. Full title/package JSON parser lands in
    // the next milestone so the transport/cache path can be tested first.
    if (!parseRevision(json, catalog.revision)) {
        error = "catalog revision missing";
        return false;
    }
    catalog.titles.clear();
    return true;
}

bool CatalogClient::fetchCatalog(Catalog& catalog, std::string& error) {
    auto r = http_.get(std::string(API_BASE) + CATALOG_PATH);
    if (!r.ok()) {
        error = r.error.empty() ? ("HTTP " + std::to_string(r.status)) : r.error;
        return false;
    }
    if (!parseCatalog(r.body, catalog, error)) return false;
    saveRawCache(r.body, error);
    return true;
}

bool CatalogClient::loadCached(Catalog& catalog, std::string& error) {
    std::string raw;
    if (!read_all(CACHE_FILE, raw)) {
        error = "cache not found";
        return false;
    }
    return parseCatalog(raw, catalog, error);
}

bool CatalogClient::saveRawCache(const std::string& json, std::string& error) {
    if (!write_all(CACHE_FILE, json)) {
        error = "failed to write cache";
        return false;
    }
    return true;
}

} // namespace ov
