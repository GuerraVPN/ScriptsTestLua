#pragma once
#include <string>

namespace ov {

struct HttpResponse {
    long status = 0;
    std::string body;
    std::string error;

    bool ok() const { return status >= 200 && status < 300; }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse get(const std::string& url, const std::string& bearer = "");
    HttpResponse postJson(const std::string& url, const std::string& json,
                          const std::string& bearer = "");
    HttpResponse patchJson(const std::string& url, const std::string& json,
                           const std::string& bearer = "");

private:
    HttpResponse request(const char* method, const std::string& url,
                         const std::string& body, const std::string& bearer);
};

} // namespace ov
