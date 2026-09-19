#include "remote_queue.hpp"
#include "config.hpp"
#include "third_party/jsmn.h"

#include <stdlib.h>
#include <vector>

namespace ov {

RemoteQueueClient::RemoteQueueClient(HttpClient& http) : http_(http) {}

static std::string tokText(const std::string& json, const jsmntok_t& t) {
    if (t.start < 0 || t.end < t.start) return "";
    return json.substr(static_cast<size_t>(t.start),
                       static_cast<size_t>(t.end - t.start));
}

static int nextToken(const std::vector<jsmntok_t>& tokens, int index) {
    const int end = tokens[index].end;
    int i = index + 1;
    while (i < static_cast<int>(tokens.size()) && tokens[i].start < end) ++i;
    return i;
}

static int valueFor(const std::string& json,
                    const std::vector<jsmntok_t>& tokens,
                    int objectIndex,
                    const char* key) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(tokens.size()) ||
        tokens[objectIndex].type != JSMN_OBJECT) return -1;

    int i = objectIndex + 1;
    while (i + 1 < static_cast<int>(tokens.size()) &&
           tokens[i].start < tokens[objectIndex].end) {
        const int value = i + 1;
        if (tokens[i].type == JSMN_STRING &&
            tokText(json, tokens[i]) == key) return value;
        i = nextToken(tokens, value);
    }
    return -1;
}

static bool tokenizeJson(const std::string& json,
                         std::vector<jsmntok_t>& tokens) {
    unsigned int capacity = 512;
    for (int attempt=0; attempt<5; ++attempt) {
        tokens.assign(capacity, {});
        jsmn_parser parser;
        jsmn_init(&parser);
        const int count = jsmn_parse(
            &parser, json.c_str(), json.size(), tokens.data(), capacity);
        if (count >= 0) {
            tokens.resize(static_cast<size_t>(count));
            return true;
        }
        if (count != JSMN_ERROR_NOMEM) return false;
        capacity *= 2;
    }
    return false;
}

static std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size()+8);
    for (char c : input) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

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

    std::vector<jsmntok_t> tokens;
    if (!tokenizeJson(r.body, tokens) || tokens.empty()) {
        error = "invalid remote command JSON";
        return false;
    }

    const int commandsIndex = valueFor(r.body, tokens, 0, "commands");
    if (commandsIndex < 0 || tokens[commandsIndex].type != JSMN_ARRAY) {
        error = "commands array missing";
        return false;
    }

    int i = commandsIndex + 1;
    while (i < static_cast<int>(tokens.size()) &&
           tokens[i].start < tokens[commandsIndex].end) {
        if (tokens[i].type == JSMN_OBJECT) {
            RemoteCommand cmd;

            const int idIdx = valueFor(r.body, tokens, i, "id");
            const int actionIdx = valueFor(r.body, tokens, i, "action");
            const int titleDbIdx = valueFor(r.body, tokens, i, "title_db_id");
            const int titleIdIdx = valueFor(r.body, tokens, i, "title_id");
            const int packageIdsIdx = valueFor(r.body, tokens, i, "package_ids");

            if (idIdx >= 0)
                cmd.id = strtol(tokText(r.body, tokens[idIdx]).c_str(), nullptr, 10);
            if (actionIdx >= 0)
                cmd.action = tokText(r.body, tokens[actionIdx]);
            if (titleDbIdx >= 0)
                cmd.titleDbId = atoi(tokText(r.body, tokens[titleDbIdx]).c_str());
            if (titleIdIdx >= 0 && tokens[titleIdIdx].type == JSMN_STRING)
                cmd.titleId = tokText(r.body, tokens[titleIdIdx]);

            if (packageIdsIdx >= 0 &&
                tokens[packageIdsIdx].type == JSMN_ARRAY) {
                int p = packageIdsIdx + 1;
                while (p < static_cast<int>(tokens.size()) &&
                       tokens[p].start < tokens[packageIdsIdx].end) {
                    if (tokens[p].type == JSMN_PRIMITIVE)
                        cmd.packageIds.push_back(
                            atoi(tokText(r.body, tokens[p]).c_str()));
                    p = nextToken(tokens, p);
                }
            }

            if (cmd.id > 0 && !cmd.action.empty())
                commands.push_back(cmd);
        }
        i = nextToken(tokens, i);
    }

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
        "{\"status\":\"" + jsonEscape(status) + "\",\"progress\":" +
        std::to_string(progress) + ",\"message\":\"" +
        jsonEscape(message) + "\"}";

    auto r = http_.patchJson(url, body, device.token);
    if (!r.ok()) {
        error = r.error.empty() ? ("HTTP " + std::to_string(r.status)) : r.error;
        return false;
    }
    return true;
}

bool RemoteQueueClient::heartbeat(const DeviceIdentity& device,
                                  std::string& error) {
    if (!device.paired || device.token.empty()) {
        error = "device is not paired";
        return false;
    }

    const std::string body =
        "{\"device_id\":\"" + jsonEscape(device.deviceId) + "\"}";

    auto r = http_.postJson(
        std::string(API_BASE) + "/api/device/heartbeat",
        body,
        device.token);

    if (!r.ok()) {
        error = r.error.empty() ? ("HTTP " + std::to_string(r.status)) : r.error;
        return false;
    }
    return true;
}

} // namespace ov
