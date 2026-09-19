#pragma once
#include "http_client.hpp"
#include "models.hpp"
#include <string>

namespace ov {

class CoverCache {
public:
    explicit CoverCache(HttpClient& http);

    bool ensure(const TitleItem& title, std::string& error);
    void sync(const Catalog& catalog);

    static std::string pathFor(const TitleItem& title);

private:
    HttpClient& http_;
};

} // namespace ov
