#include "http_client.hpp"
#include "config.hpp"
#include <curl/curl.h>

namespace ov {

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

HttpResponse HttpClient::request(const char* method, const std::string& url,
                                 const std::string& body,
                                 const std::string& bearer) {
    HttpResponse result;
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "curl_easy_init failed";
        return result;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");

    if (body.size()) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }

    std::string auth;
    if (!bearer.empty()) {
        auth = "Authorization: Bearer " + bearer;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_TIMEOUT_SECONDS);

    if (std::string(method) != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        result.error = curl_easy_strerror(rc);
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

HttpResponse HttpClient::get(const std::string& url, const std::string& bearer) {
    return request("GET", url, "", bearer);
}

HttpResponse HttpClient::postJson(const std::string& url, const std::string& json,
                                  const std::string& bearer) {
    return request("POST", url, json, bearer);
}

HttpResponse HttpClient::patchJson(const std::string& url, const std::string& json,
                                   const std::string& bearer) {
    return request("PATCH", url, json, bearer);
}

} // namespace ov
