#include "download_manager.hpp"
#include "native_http.hpp"
#include "ps4_http_api.hpp"

#include <stdio.h>
#include <sys/stat.h>

namespace ov {

static constexpr const char* USER_AGENT = "OrbisVaultSafe/0.2 (PS4)";

static uint64_t localFileSize(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_size);
}

DownloadResult DownloadManager::download(const PackageItem& pkg,
                                         const std::string& destination,
                                         ProgressCallback progress,
                                         CancelCallback shouldCancel,
                                         bool resume) {
    DownloadResult out;
    out.localPath = destination;

    if (pkg.sourceUrl.empty()) {
        out.error = "package URL is empty";
        return out;
    }

    std::string initError;
    if (!ensureNativeHttp(&initError)) {
        out.error = initError.empty() ? "native HTTP unavailable" : initError;
        return out;
    }

    auto& api = nativeHttpApi();
    uint64_t existing = resume ? localFileSize(destination) : 0;

    int tpl = api.httpCreateTemplate(
        nativeHttpContext(), USER_AGENT, OV_HTTP_VERSION_1_1, 1);
    if (tpl < 0) {
        out.error = "sceHttpCreateTemplate failed";
        return out;
    }

    int conn = api.httpCreateConnectionWithURL(tpl, pkg.sourceUrl.c_str(), true);
    if (conn < 0) {
        api.httpDeleteTemplate(tpl);
        out.error = "sceHttpCreateConnectionWithURL failed";
        return out;
    }

    int req = api.httpCreateRequestWithURL(
        conn, OV_HTTP_METHOD_GET, pkg.sourceUrl.c_str(), 0);
    if (req < 0) {
        api.httpDeleteConnection(conn);
        api.httpDeleteTemplate(tpl);
        out.error = "sceHttpCreateRequestWithURL failed";
        return out;
    }

    if (existing > 0) {
        const std::string range = "bytes=" + std::to_string(existing) + "-";
        api.httpAddRequestHeader(req, "Range", range.c_str(), 0);
    }

    int rc = api.httpSendRequest(req, nullptr, 0);
    if (rc < 0) {
        out.error = "sceHttpSendRequest failed";
        api.httpDeleteRequest(req);
        api.httpDeleteConnection(conn);
        api.httpDeleteTemplate(tpl);
        return out;
    }

    int32_t status = 0;
    api.httpGetStatusCode(req, &status);
    out.httpStatus = status;

    if (status != 200 && status != 206) {
        out.error = "HTTP " + std::to_string(status);
        api.httpDeleteRequest(req);
        api.httpDeleteConnection(conn);
        api.httpDeleteTemplate(tpl);
        return out;
    }

    const bool continued = (existing > 0 && status == 206);
    if (!continued) existing = 0;

    FILE* file = fopen(destination.c_str(), continued ? "ab" : "wb");
    if (!file) {
        out.error = "cannot open destination";
        api.httpDeleteRequest(req);
        api.httpDeleteConnection(conn);
        api.httpDeleteTemplate(tpl);
        return out;
    }

    int contentLengthType = 0;
    size_t responseLength = 0;
    api.httpGetResponseContentLength(req, &contentLengthType, &responseLength);

    const uint64_t total =
        contentLengthType == OV_HTTP_CONTENTLEN_EXIST
            ? existing + static_cast<uint64_t>(responseLength)
            : pkg.sizeBytes;

    uint8_t buffer[64 * 1024];
    uint64_t downloaded = existing;

    for (;;) {
        if (shouldCancel && shouldCancel()) {
            out.cancelled = true;
            out.error = "cancelled";
            break;
        }

        const int read = api.httpReadData(req, buffer, sizeof(buffer));
        if (read < 0) {
            out.error = "sceHttpReadData failed";
            break;
        }
        if (read == 0) {
            out.ok = true;
            break;
        }

        const size_t written = fwrite(buffer, 1, static_cast<size_t>(read), file);
        if (written != static_cast<size_t>(read)) {
            out.error = "storage write failed";
            break;
        }

        downloaded += static_cast<uint64_t>(read);
        if (progress) progress(downloaded, total, 0.0);
    }

    fclose(file);
    api.httpDeleteRequest(req);
    api.httpDeleteConnection(conn);
    api.httpDeleteTemplate(tpl);
    return out;
}

} // namespace ov
