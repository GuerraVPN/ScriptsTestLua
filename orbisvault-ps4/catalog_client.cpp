#include "catalog_client.hpp"
#include "config.hpp"
#include "third_party/jsmn.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

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

static std::string token_text(const std::string& json, const jsmntok_t& t) {
    if (t.start < 0 || t.end < t.start) return "";
    return json.substr(static_cast<size_t>(t.start),
                       static_cast<size_t>(t.end - t.start));
}

static int next_after(const std::vector<jsmntok_t>& tokens, int index) {
    const int end = tokens[index].end;
    int i = index + 1;
    while (i < static_cast<int>(tokens.size()) && tokens[i].start < end) ++i;
    return i;
}

static int object_value(const std::string& json,
                        const std::vector<jsmntok_t>& tokens,
                        int objectIndex,
                        const char* key) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(tokens.size())) return -1;
    if (tokens[objectIndex].type != JSMN_OBJECT) return -1;

    int i = objectIndex + 1;
    while (i + 1 < static_cast<int>(tokens.size()) &&
           tokens[i].start < tokens[objectIndex].end) {
        if (tokens[i].type != JSMN_STRING) return -1;
        const int valueIndex = i + 1;
        if (token_text(json, tokens[i]) == key) return valueIndex;
        i = next_after(tokens, valueIndex);
    }
    return -1;
}

static int as_int(const std::string& json,
                  const std::vector<jsmntok_t>& tokens,
                  int index,
                  int fallback = 0) {
    if (index < 0) return fallback;
    return atoi(token_text(json, tokens[index]).c_str());
}

static uint64_t as_u64(const std::string& json,
                       const std::vector<jsmntok_t>& tokens,
                       int index,
                       uint64_t fallback = 0) {
    if (index < 0) return fallback;
    return static_cast<uint64_t>(strtoull(token_text(json, tokens[index]).c_str(), nullptr, 10));
}

static bool as_bool(const std::string& json,
                    const std::vector<jsmntok_t>& tokens,
                    int index,
                    bool fallback = false) {
    if (index < 0) return fallback;
    const auto v = token_text(json, tokens[index]);
    return v == "true" || v == "1";
}

static std::string as_string(const std::string& json,
                             const std::vector<jsmntok_t>& tokens,
                             int index,
                             const std::string& fallback = "") {
    if (index < 0) return fallback;
    return token_text(json, tokens[index]);
}

static PackageType package_type_from_string(const std::string& v) {
    if (v == "BASE") return PackageType::Base;
    if (v == "UPDATE") return PackageType::Update;
    if (v == "DLC") return PackageType::Dlc;
    return PackageType::Unknown;
}

static bool parse_package(const std::string& json,
                          const std::vector<jsmntok_t>& tokens,
                          int objectIndex,
                          PackageItem& out) {
    if (objectIndex < 0 || tokens[objectIndex].type != JSMN_OBJECT) return false;

    out.id = as_int(json, tokens, object_value(json, tokens, objectIndex, "id"));
    const std::string type = as_string(
        json, tokens, object_value(json, tokens, objectIndex, "package_type"));
    out.type = package_type_from_string(type);
    out.name = as_string(json, tokens, object_value(json, tokens, objectIndex, "name"));
    out.version = as_string(json, tokens, object_value(json, tokens, objectIndex, "version"));
    out.requiredBaseVersion = as_string(
        json, tokens, object_value(json, tokens, objectIndex, "required_base_version"));
    out.sourceUrl = as_string(
        json, tokens, object_value(json, tokens, objectIndex, "source_url"));
    out.sizeBytes = as_u64(
        json, tokens, object_value(json, tokens, objectIndex, "size_bytes"));
    out.sha256 = as_string(
        json, tokens, object_value(json, tokens, objectIndex, "sha256"));

    return out.id > 0 || !out.name.empty() || !out.version.empty();
}

static void parse_package_array(const std::string& json,
                                const std::vector<jsmntok_t>& tokens,
                                int arrayIndex,
                                std::vector<PackageItem>& out) {
    out.clear();
    if (arrayIndex < 0 || tokens[arrayIndex].type != JSMN_ARRAY) return;

    int i = arrayIndex + 1;
    while (i < static_cast<int>(tokens.size()) &&
           tokens[i].start < tokens[arrayIndex].end) {
        if (tokens[i].type == JSMN_OBJECT) {
            PackageItem p;
            if (parse_package(json, tokens, i, p)) out.push_back(p);
        }
        i = next_after(tokens, i);
    }
}

static bool parse_title(const std::string& json,
                        const std::vector<jsmntok_t>& tokens,
                        int objectIndex,
                        TitleItem& out) {
    if (objectIndex < 0 || tokens[objectIndex].type != JSMN_OBJECT) return false;

    out.id = as_int(json, tokens, object_value(json, tokens, objectIndex, "id"));
    out.titleId = as_string(json, tokens, object_value(json, tokens, objectIndex, "title_id"));
    out.name = as_string(json, tokens, object_value(json, tokens, objectIndex, "name"));
    out.description = as_string(
        json, tokens, object_value(json, tokens, objectIndex, "description"));
    out.category = as_string(
        json, tokens, object_value(json, tokens, objectIndex, "category"));
    out.region = as_string(json, tokens, object_value(json, tokens, objectIndex, "region"));
    out.coverUrl = as_string(
        json, tokens, object_value(json, tokens, objectIndex, "cover_url"));
    out.featured = as_bool(
        json, tokens, object_value(json, tokens, objectIndex, "featured"));

    const int baseIndex = object_value(json, tokens, objectIndex, "base");
    out.hasBase = false;
    if (baseIndex >= 0 && tokens[baseIndex].type == JSMN_OBJECT) {
        PackageItem base;
        if (parse_package(json, tokens, baseIndex, base)) {
            out.base = base;
            out.hasBase = true;
        }
    }

    parse_package_array(
        json, tokens, object_value(json, tokens, objectIndex, "updates"), out.updates);
    parse_package_array(
        json, tokens, object_value(json, tokens, objectIndex, "dlcs"), out.dlcs);

    return out.id > 0 && !out.titleId.empty() && !out.name.empty();
}

static bool tokenize(const std::string& json,
                     std::vector<jsmntok_t>& tokens,
                     std::string& error) {
    unsigned int capacity = 1024;
    for (int attempt = 0; attempt < 5; ++attempt) {
        tokens.assign(capacity, {});
        jsmn_parser parser;
        jsmn_init(&parser);
        const int count = jsmn_parse(
            &parser, json.c_str(), json.size(), tokens.data(), capacity);
        if (count >= 0) {
            tokens.resize(static_cast<size_t>(count));
            return true;
        }
        if (count != JSMN_ERROR_NOMEM) {
            error = "invalid JSON";
            return false;
        }
        capacity *= 2;
    }
    error = "catalog JSON too large";
    return false;
}

bool CatalogClient::parseRevision(const std::string& json, int& revision) {
    std::vector<jsmntok_t> tokens;
    std::string error;
    if (!tokenize(json, tokens, error) || tokens.empty()) return false;
    const int idx = object_value(json, tokens, 0, "revision");
    if (idx < 0) return false;
    revision = as_int(json, tokens, idx, -1);
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

bool CatalogClient::parseCatalog(const std::string& json,
                                 Catalog& catalog,
                                 std::string& error) {
    std::vector<jsmntok_t> tokens;
    if (!tokenize(json, tokens, error) || tokens.empty()) return false;
    if (tokens[0].type != JSMN_OBJECT) {
        error = "catalog root must be an object";
        return false;
    }

    const int revisionIndex = object_value(json, tokens, 0, "revision");
    const int catalogIndex = object_value(json, tokens, 0, "catalog");

    if (revisionIndex < 0 || catalogIndex < 0 ||
        tokens[catalogIndex].type != JSMN_ARRAY) {
        error = "catalog payload missing revision/catalog";
        return false;
    }

    Catalog parsed;
    parsed.revision = as_int(json, tokens, revisionIndex, 0);

    int i = catalogIndex + 1;
    while (i < static_cast<int>(tokens.size()) &&
           tokens[i].start < tokens[catalogIndex].end) {
        if (tokens[i].type == JSMN_OBJECT) {
            TitleItem t;
            if (parse_title(json, tokens, i, t)) parsed.titles.push_back(t);
        }
        i = next_after(tokens, i);
    }

    catalog = parsed;
    return true;
}

bool CatalogClient::fetchCatalog(Catalog& catalog, std::string& error) {
    auto r = http_.get(std::string(API_BASE) + CATALOG_PATH);
    if (!r.ok()) {
        error = r.error.empty() ? ("HTTP " + std::to_string(r.status)) : r.error;
        return false;
    }
    if (!parseCatalog(r.body, catalog, error)) return false;

    std::string cacheError;
    saveRawCache(r.body, cacheError);
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
