#include "cover_cache.hpp"
#include "config.hpp"

#include <stdio.h>
#include <sys/stat.h>

namespace ov {

CoverCache::CoverCache(HttpClient& http) : http_(http) {}

static bool file_exists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && st.st_size > 0;
}

std::string CoverCache::pathFor(const TitleItem& title) {
    return std::string(DATA_DIR) + "/covers/" + title.titleId + ".img";
}

bool CoverCache::ensure(const TitleItem& title, std::string& error) {
    if (title.coverUrl.empty()) return false;

    const std::string path = pathFor(title);
    if (file_exists(path)) return true;

    auto r = http_.get(title.coverUrl);
    if (!r.ok() || r.body.empty()) {
        error = r.error.empty()
            ? ("cover HTTP " + std::to_string(r.status))
            : r.error;
        return false;
    }

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        error = "cannot create cover cache file";
        return false;
    }

    const size_t written = fwrite(r.body.data(), 1, r.body.size(), f);
    fclose(f);

    if (written != r.body.size()) {
        error = "cover cache write failed";
        return false;
    }
    return true;
}

void CoverCache::sync(const Catalog& catalog) {
    for (const auto& title : catalog.titles) {
        std::string ignored;
        ensure(title, ignored);
    }
}

} // namespace ov
