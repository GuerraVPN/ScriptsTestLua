#pragma once
#include "models.hpp"
#include <functional>
#include <string>

namespace ov {

using ProgressCallback = std::function<void(uint64_t downloaded,
                                            uint64_t total,
                                            double bytesPerSecond)>;
using CancelCallback = std::function<bool()>;

struct DownloadResult {
    bool ok = false;
    long httpStatus = 0;
    std::string localPath;
    std::string error;
    bool cancelled = false;
};

class DownloadManager {
public:
    DownloadResult download(const PackageItem& pkg,
                            const std::string& destination,
                            ProgressCallback progress,
                            CancelCallback shouldCancel = {},
                            bool resume = true);
};

} // namespace ov
