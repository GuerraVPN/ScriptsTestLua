#pragma once
#include "download_manager.hpp"
#include "installer.hpp"
#include "models.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace ov {

struct InstallSnapshot {
    bool active = false;
    bool completed = false;
    bool failed = false;
    int progress = 0;
    std::string title;
    std::string titleId;
    std::string stage;
    std::string error;
};

class InstallCoordinator {
public:
    explicit InstallCoordinator(Installer& installer);
    ~InstallCoordinator();

    bool start(const TitleItem& title);
    void cancel();
    void shutdown();
    InstallSnapshot snapshot() const;

private:
    Installer& installer_;
    DownloadManager downloader_;
    std::thread worker_;
    std::atomic<bool> cancelRequested_{false};
    std::atomic<bool> active_{false};

    mutable std::mutex stateMutex_;
    InstallSnapshot state_;

    void run(TitleItem title);
    void setState(const std::string& stage, int progress,
                  bool completed = false, bool failed = false,
                  const std::string& error = "");
};

} // namespace ov
