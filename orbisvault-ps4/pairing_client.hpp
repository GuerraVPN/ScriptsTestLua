#pragma once
#include "http_client.hpp"
#include "models.hpp"
#include <string>

namespace ov {

struct PairingResult {
    bool ok = false;
    std::string code;
    std::string error;
};

class PairingClient {
public:
    explicit PairingClient(HttpClient& http);

    bool load(DeviceIdentity& device);
    bool save(const DeviceIdentity& device);

    PairingResult start(DeviceIdentity& device,
                        const std::string& deviceName = "Orbis Vault PS4");

    bool refreshStatus(DeviceIdentity& device, std::string& error);

private:
    HttpClient& http_;

    std::string generateDeviceId();
    static std::string jsonString(const std::string& json,
                                  const std::string& key);
    static bool jsonBool(const std::string& json,
                         const std::string& key,
                         bool fallback);
};

} // namespace ov
