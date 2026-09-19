#pragma once
#include "models.hpp"
#include "http_client.hpp"
#include <string>

namespace ov {

class CatalogClient {
public:
    explicit CatalogClient(HttpClient& http);

    bool fetchRevision(int& revision, std::string& error);
    bool fetchCatalog(Catalog& catalog, std::string& error);

    bool loadCached(Catalog& catalog, std::string& error);
    bool saveRawCache(const std::string& json, std::string& error);

private:
    HttpClient& http_;
    bool parseRevision(const std::string& json, int& revision);
    bool parseCatalog(const std::string& json, Catalog& catalog, std::string& error);
};

} // namespace ov
