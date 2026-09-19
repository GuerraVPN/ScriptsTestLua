#pragma once
#include "models.hpp"
#include "http_client.hpp"
#include <string>
#include <vector>

namespace ov {

struct RemoteCommand {
    long id = 0;
    std::string action;
    int titleDbId = 0;
    std::vector<int> packageIds;
};

class RemoteQueueClient {
public:
    explicit RemoteQueueClient(HttpClient& http);

    bool poll(const DeviceIdentity& device,
              std::vector<RemoteCommand>& commands,
              std::string& error);

    bool updateStatus(const DeviceIdentity& device,
                      long commandId,
                      const std::string& status,
                      int progress,
                      const std::string& message,
                      std::string& error);

private:
    HttpClient& http_;
};

} // namespace ov
