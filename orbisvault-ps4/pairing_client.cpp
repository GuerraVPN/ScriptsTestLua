#include "pairing_client.hpp"
#include "config.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace ov {

PairingClient::PairingClient(HttpClient& http) : http_(http) {}

static std::string escapeJson(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string PairingClient::jsonString(const std::string& json,
                                      const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return "";
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return "";
    p = json.find('"', p + 1);
    if (p == std::string::npos) return "";
    size_t e = p + 1;
    std::string out;
    bool escaped = false;
    for (; e < json.size(); ++e) {
        char c = json[e];
        if (escaped) {
            out.push_back(c);
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return out;
        } else {
            out.push_back(c);
        }
    }
    return "";
}

bool PairingClient::jsonBool(const std::string& json,
                             const std::string& key,
                             bool fallback) {
    const std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return fallback;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return fallback;
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    if (json.compare(p, 4, "true") == 0) return true;
    if (json.compare(p, 5, "false") == 0) return false;
    return fallback;
}

std::string PairingClient::generateDeviceId() {
    const uint64_t now = static_cast<uint64_t>(time(nullptr));
    const uint64_t mix =
        (now * 0x9E3779B185EBCA87ULL) ^
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));

    char buf[64];
    snprintf(buf, sizeof(buf), "ps4_%016llx_%08x",
             static_cast<unsigned long long>(mix),
             static_cast<unsigned int>((mix >> 17) ^ now));
    return buf;
}

bool PairingClient::load(DeviceIdentity& device) {
    FILE* f = fopen(DEVICE_FILE, "rb");
    if (!f) return false;

    char line[1024];
    DeviceIdentity parsed;
    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* value = eq + 1;

        if (strcmp(key, "device_id") == 0) parsed.deviceId = value;
        else if (strcmp(key, "device_name") == 0) parsed.deviceName = value;
        else if (strcmp(key, "token") == 0) parsed.token = value;
        else if (strcmp(key, "paired") == 0) parsed.paired = strcmp(value, "1") == 0;
    }
    fclose(f);

    if (parsed.deviceId.empty()) return false;
    device = parsed;
    return true;
}

bool PairingClient::save(const DeviceIdentity& device) {
    FILE* f = fopen(DEVICE_FILE, "wb");
    if (!f) return false;

    fprintf(f, "device_id=%s\n", device.deviceId.c_str());
    fprintf(f, "device_name=%s\n", device.deviceName.c_str());
    fprintf(f, "token=%s\n", device.token.c_str());
    fprintf(f, "paired=%d\n", device.paired ? 1 : 0);
    fclose(f);
    return true;
}

PairingResult PairingClient::start(DeviceIdentity& device,
                                   const std::string& deviceName) {
    PairingResult out;

    if (device.deviceId.empty()) device.deviceId = generateDeviceId();
    device.deviceName = deviceName;

    const std::string body =
        "{\"device_id\":\"" + escapeJson(device.deviceId) +
        "\",\"device_name\":\"" + escapeJson(device.deviceName) + "\"}";

    auto r = http_.postJson(
        std::string(API_BASE) + "/api/device/pair/start", body);

    if (!r.ok()) {
        out.error = r.error.empty()
            ? ("HTTP " + std::to_string(r.status))
            : r.error;
        return out;
    }

    const std::string token = jsonString(r.body, "device_token");
    const std::string code = jsonString(r.body, "pairing_code");

    if (token.empty() || code.empty()) {
        out.error = "invalid pairing response";
        return out;
    }

    device.token = token;
    device.paired = false;
    save(device);

    out.ok = true;
    out.code = code;
    return out;
}

bool PairingClient::refreshStatus(DeviceIdentity& device,
                                  std::string& error) {
    if (device.deviceId.empty()) {
        error = "device_id missing";
        return false;
    }

    auto r = http_.get(
        std::string(API_BASE) +
        "/api/device/pair/status?device_id=" + device.deviceId);

    if (!r.ok()) {
        error = r.error.empty()
            ? ("HTTP " + std::to_string(r.status))
            : r.error;
        return false;
    }

    device.paired = jsonBool(r.body, "paired", false);
    save(device);
    return true;
}

} // namespace ov
