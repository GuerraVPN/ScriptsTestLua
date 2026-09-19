#include "remote_queue.hpp"
#include "config.hpp"

namespace ov {

RemoteQueueClient::RemoteQueueClient(HttpClient& http) : http_(http) {}

bool RemoteQueueClient::poll(const DeviceIdentity& device,
                             std::vector<RemoteCommand>& commands,
                             std::string& error) {
    commands.clear();
    if (!device.paired || device.token.empty()) {
        error = "device is not paired";
        return false;
    }

    const std::string url =
        std::string(API_BASE) + "/api/device/commands?device_id=" + device.deviceId;

    auto r = http_.get(url, device.token);
    if (!r.ok()) {
        error = r.error.empty() ? ("HTTP " + std::to_string(r.status)) : r.error;
        return false;
    }

    // Command JSON parser will be connected in the pairing milestone.
    return true;
}

bool RemoteQueueClient::updateStatus(const DeviceIdentity& device,
                                     long commandId,
                                     const std::string& status,
                                     int progress,
                                     const std::string& message,
                                     std::string& error) {
    const std::string url =
        std::string(API_BASE) + "/api/device/commands/" + std::to_string(commandId);

    const std::string body =
        "{\"status\":\"" + status + "\",\"progress\":" +
        std::to_string(progress) + ",\"message\":\"" + message + "\"}";

    auto r = http_.patchJson(url, body, device.token);
    if (!r.ok()) {
        error = r.error.empty() ? ("HTTP " + std::to_string(r.status)) : r.error;
        return false;
    }
    return true;
}

} // namespace ov
