#include "download_manager.hpp"
#include "config.hpp"
#include <curl/curl.h>
#include <stdio.h>
#include <sys/stat.h>

namespace ov {

struct DownloadContext {
    FILE* file = nullptr;
    ProgressCallback progress;
};

static size_t file_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<DownloadContext*>(userdata);
    return fwrite(ptr, size, nmemb, ctx->file);
}

static int progress_cb(void* clientp,
                       curl_off_t dltotal,
                       curl_off_t dlnow,
                       curl_off_t,
                       curl_off_t) {
    auto* ctx = static_cast<DownloadContext*>(clientp);
    if (ctx->progress) {
        ctx->progress(static_cast<uint64_t>(dlnow),
                      static_cast<uint64_t>(dltotal),
                      0.0);
    }
    return 0;
}

static curl_off_t file_size(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<curl_off_t>(st.st_size);
}

DownloadResult DownloadManager::download(const PackageItem& pkg,
                                         const std::string& destination,
                                         ProgressCallback progress,
                                         bool resume) {
    DownloadResult out;
    out.localPath = destination;

    if (pkg.sourceUrl.empty()) {
        out.error = "package URL is empty";
        return out;
    }

    curl_off_t existing = resume ? file_size(destination) : 0;
    FILE* file = fopen(destination.c_str(), existing > 0 ? "ab" : "wb");
    if (!file) {
        out.error = "cannot open destination";
        return out;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(file);
        out.error = "curl_easy_init failed";
        return out;
    }

    DownloadContext ctx;
    ctx.file = file;
    ctx.progress = progress;

    curl_easy_setopt(curl, CURLOPT_URL, pkg.sourceUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, HTTP_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);

    if (existing > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, existing);
    }

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.httpStatus);

    if (rc == CURLE_OK && out.httpStatus >= 200 && out.httpStatus < 400) {
        out.ok = true;
    } else {
        out.error = curl_easy_strerror(rc);
    }

    curl_easy_cleanup(curl);
    fclose(file);
    return out;
}

} // namespace ov
