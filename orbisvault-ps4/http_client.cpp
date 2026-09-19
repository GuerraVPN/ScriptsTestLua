#include "http_client.hpp"
#include "native_http.hpp"

#include "ps4_http_api.hpp"
#include <stdio.h>

namespace ov {

static constexpr const char* USER_AGENT = "OrbisVault/0.1 (PlayStation 4)";

HttpClient::HttpClient() {
    ensureNativeHttp();
}

HttpClient::~HttpClient() {
    // Runtime is shared with the streaming downloader and remains alive
    // for the application lifetime. main() performs final shutdown.
}

HttpResponse HttpClient::request(const char* method,
                                 const std::string& url,
                                 const std::string& body,
                                 const std::string& bearer) {
    HttpResponse result;

    if (!ensureNativeHttp()) {
        result.error = "native HTTP initialization failed";
        return result;
    }

    int tpl = sceHttpCreateTemplate(
        nativeHttpContext(), USER_AGENT, OV_HTTP_VERSION_1_1, 1);
    if (tpl < 0) {
        result.error = "sceHttpCreateTemplate failed";
        return result;
    }

    int conn = sceHttpCreateConnectionWithURL(tpl, url.c_str(), true);
    if (conn < 0) {
        sceHttpDeleteTemplate(tpl);
        result.error = "sceHttpCreateConnectionWithURL failed";
        return result;
    }

    const uint64_t contentLength = static_cast<uint64_t>(body.size());
    int req = sceHttpCreateRequestWithURL2(
        conn, method, url.c_str(), contentLength);

    if (req < 0) {
        sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tpl);
        result.error = "sceHttpCreateRequestWithURL2 failed";
        return result;
    }

    sceHttpAddRequestHeader(req, "Accept", "application/json", 0);

    if (!body.empty()) {
        sceHttpAddRequestHeader(
            req, "Content-Type", "application/json; charset=utf-8", 0);
    }

    std::string auth;
    if (!bearer.empty()) {
        auth = "Bearer " + bearer;
        sceHttpAddRequestHeader(req, "Authorization", auth.c_str(), 0);
    }

    const void* sendBody = body.empty() ? nullptr : body.data();
    const size_t sendSize = body.size();

    int rc = sceHttpSendRequest(req, sendBody, sendSize);
    if (rc < 0) {
        result.error = "sceHttpSendRequest failed";
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tpl);
        return result;
    }

    int32_t status = 0;
    if (sceHttpGetStatusCode(req, &status) >= 0) {
        result.status = static_cast<long>(status);
    }

    char buffer[32 * 1024];
    for (;;) {
        const int read = sceHttpReadData(req, buffer, sizeof(buffer));
        if (read < 0) {
            result.error = "sceHttpReadData failed";
            break;
        }
        if (read == 0) break;
        result.body.append(buffer, static_cast<size_t>(read));
    }

    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return result;
}

HttpResponse HttpClient::get(const std::string& url,
                             const std::string& bearer) {
    return request("GET", url, "", bearer);
}

HttpResponse HttpClient::postJson(const std::string& url,
                                  const std::string& json,
                                  const std::string& bearer) {
    return request("POST", url, json, bearer);
}

HttpResponse HttpClient::patchJson(const std::string& url,
                                   const std::string& json,
                                   const std::string& bearer) {
    return request("PATCH", url, json, bearer);
}

} // namespace ov
