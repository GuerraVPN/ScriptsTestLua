#include "http_client.hpp"
#include "native_http.hpp"
#include "ps4_http_api.hpp"

namespace ov {

static constexpr const char* USER_AGENT = "OrbisVaultSafe/0.2 (PS4)";

HttpClient::HttpClient() = default;
HttpClient::~HttpClient() = default;

HttpResponse HttpClient::request(const char* method,
                                 const std::string& url,
                                 const std::string& body,
                                 const std::string& bearer) {
    HttpResponse result;
    std::string initError;

    if (!ensureNativeHttp(&initError)) {
        result.error = initError.empty() ? "native HTTP unavailable" : initError;
        return result;
    }

    auto& api = nativeHttpApi();

    int tpl = api.httpCreateTemplate(
        nativeHttpContext(), USER_AGENT, OV_HTTP_VERSION_1_1, 1);
    if (tpl < 0) {
        result.error = "sceHttpCreateTemplate failed";
        return result;
    }

    int conn = api.httpCreateConnectionWithURL(tpl, url.c_str(), true);
    if (conn < 0) {
        api.httpDeleteTemplate(tpl);
        result.error = "sceHttpCreateConnectionWithURL failed";
        return result;
    }

    const uint64_t contentLength = static_cast<uint64_t>(body.size());
    int req = api.httpCreateRequestWithURL2(
        conn, method, url.c_str(), contentLength);

    if (req < 0) {
        api.httpDeleteConnection(conn);
        api.httpDeleteTemplate(tpl);
        result.error = "sceHttpCreateRequestWithURL2 failed";
        return result;
    }

    api.httpAddRequestHeader(req, "Accept", "application/json", 0);
    if (!body.empty())
        api.httpAddRequestHeader(req, "Content-Type", "application/json; charset=utf-8", 0);

    std::string auth;
    if (!bearer.empty()) {
        auth = "Bearer " + bearer;
        api.httpAddRequestHeader(req, "Authorization", auth.c_str(), 0);
    }

    const void* sendBody = body.empty() ? nullptr : body.data();
    const size_t sendSize = body.size();

    int rc = api.httpSendRequest(req, sendBody, sendSize);
    if (rc < 0) {
        result.error = "sceHttpSendRequest failed";
        api.httpDeleteRequest(req);
        api.httpDeleteConnection(conn);
        api.httpDeleteTemplate(tpl);
        return result;
    }

    int32_t status = 0;
    if (api.httpGetStatusCode(req, &status) >= 0)
        result.status = static_cast<long>(status);

    char buffer[32 * 1024];
    for (;;) {
        const int read = api.httpReadData(req, buffer, sizeof(buffer));
        if (read < 0) {
            result.error = "sceHttpReadData failed";
            break;
        }
        if (read == 0) break;
        result.body.append(buffer, static_cast<size_t>(read));
    }

    api.httpDeleteRequest(req);
    api.httpDeleteConnection(conn);
    api.httpDeleteTemplate(tpl);
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
