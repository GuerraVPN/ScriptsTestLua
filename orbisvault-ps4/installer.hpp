#pragma once
#include <string>

namespace ov {

struct InstallResult {
    bool ok = false;
    int code = 0;
    std::string message;
};

class Installer {
public:
    bool initialize();
    InstallResult installLocalPackage(const std::string& pkgPath,
                                      const std::string& displayName);
    void shutdown();
};

} // namespace ov
