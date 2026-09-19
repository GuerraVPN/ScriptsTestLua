#pragma once
#include <stdint.h>
#include <string>
#include <vector>

namespace ov {

enum class PackageType { Base, Update, Dlc, Unknown };

struct PackageItem {
    int id = 0;
    PackageType type = PackageType::Unknown;
    std::string name;
    std::string version;
    std::string requiredBaseVersion;
    std::string sourceUrl;
    uint64_t sizeBytes = 0;
    std::string sha256;
};

struct TitleItem {
    int id = 0;
    std::string titleId;
    std::string name;
    std::string description;
    std::string category;
    std::string region;
    std::string coverUrl;
    bool featured = false;

    PackageItem base;
    bool hasBase = false;
    std::vector<PackageItem> updates;
    std::vector<PackageItem> dlcs;
};

struct Catalog {
    int revision = 0;
    std::vector<TitleItem> titles;
};

enum class JobState {
    Queued,
    Downloading,
    Verifying,
    Installing,
    Completed,
    Error,
    Cancelled
};

struct InstallJob {
    std::string jobId;
    int titleDbId = 0;
    std::string titleId;
    std::string titleName;
    std::vector<PackageItem> packages;
    JobState state = JobState::Queued;
    int progress = 0;
    std::string message;
};

struct DeviceIdentity {
    std::string deviceId;
    std::string deviceName;
    std::string token;
    bool paired = false;
};

} // namespace ov
