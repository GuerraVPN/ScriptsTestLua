#include "install_coordinator.hpp"
#include "config.hpp"
#include "sha256.hpp"

#include <algorithm>
#include <cctype>
#include <stdio.h>
#include <unistd.h>
#include <vector>

namespace ov {

InstallCoordinator::InstallCoordinator(Installer& installer)
    : installer_(installer) {}

InstallCoordinator::~InstallCoordinator() {
    cancelRequested_ = true;
    if (worker_.joinable()) worker_.join();
}

void InstallCoordinator::setState(const std::string& stage, int progress,
                                  bool completed, bool failed,
                                  const std::string& error) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_.stage = stage;
    state_.progress = std::max(0, std::min(100, progress));
    state_.completed = completed;
    state_.failed = failed;
    state_.error = error;
    state_.active = active_;
}

static const char* packageLabel(PackageType type) {
    switch (type) {
        case PackageType::Base: return "BASE";
        case PackageType::Update: return "UPDATE";
        case PackageType::Dlc: return "DLC";
        default: return "PACOTE";
    }
}

static std::string destinationFor(const PackageItem& p) {
    return std::string(DOWNLOAD_DIR) + "/package_" +
           std::to_string(p.id) + ".pkg";
}

bool InstallCoordinator::start(const TitleItem& title) {
    if (active_) return false;

    if (worker_.joinable()) worker_.join();

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_ = {};
        state_.active = true;
        state_.title = title.name;
        state_.titleId = title.titleId;
        state_.stage = "PREPARANDO";
    }

    cancelRequested_ = false;
    active_ = true;
    worker_ = std::thread(&InstallCoordinator::run, this, title);
    return true;
}

void InstallCoordinator::cancel() {
    cancelRequested_ = true;
}

InstallSnapshot InstallCoordinator::snapshot() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    InstallSnapshot out = state_;
    out.active = active_;
    return out;
}

void InstallCoordinator::run(TitleItem title) {
    std::vector<PackageItem> packages;

    // Reinstalling a base that is already present is unnecessary for the
    // default update flow. Explicit remote package selection sets hasBase
    // only when Base was actually selected.
    if (title.hasBase && !title.base.sourceUrl.empty() && !title.installed)
        packages.push_back(title.base);

    // API keeps updates in insertion order. INSTALL_ALL applies the latest
    // registered update rather than every historical update.
    if (!title.updates.empty()) {
        const PackageItem* latest = nullptr;
        for (const auto& p : title.updates) {
            if (p.sourceUrl.empty()) continue;
            if (!latest || p.version > latest->version) latest = &p;
        }
        if (latest) packages.push_back(*latest);
    }

    for (const auto& dlc : title.dlcs)
        if (!dlc.sourceUrl.empty()) packages.push_back(dlc);

    if (packages.empty()) {
        active_ = false;
        setState("ERRO", 0, false, true, "Nenhum pacote com URL disponivel");
        return;
    }

    const int totalPackages = static_cast<int>(packages.size());

    for (int index = 0; index < totalPackages; ++index) {
        if (cancelRequested_) {
            active_ = false;
            setState("CANCELADO", index * 100 / totalPackages, false, true,
                     "Instalacao cancelada");
            return;
        }

        const PackageItem& pkg = packages[index];
        const std::string label = packageLabel(pkg.type);
        const std::string path = destinationFor(pkg);

        setState("BAIXANDO " + label,
                 index * 100 / totalPackages);

        auto result = downloader_.download(
            pkg, path,
            [this,index,totalPackages,label](uint64_t now,uint64_t total,double) {
                int pkgPct = total > 0 ? int((now * 100) / total) : 0;
                int overall = ((index * 100) + pkgPct) / totalPackages;
                setState("BAIXANDO " + label, overall);
            },
            [this]() { return cancelRequested_.load(); },
            true);

        if (!result.ok) {
            active_ = false;
            if (result.cancelled || cancelRequested_) {
                setState("CANCELADO", index * 100 / totalPackages, false, true,
                         "Instalacao cancelada");
            } else {
                setState("ERRO", index * 100 / totalPackages, false, true,
                         "Download falhou: " + result.error);
            }
            return;
        }

        if (!pkg.sha256.empty()) {
            setState("VERIFICANDO " + label,
                     ((index * 100) + 95) / totalPackages);

            std::string expected = pkg.sha256;
            std::transform(expected.begin(), expected.end(), expected.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            const std::string actual = sha256File(path);
            if (actual.empty() || actual != expected) {
                active_ = false;
                setState("ERRO", index * 100 / totalPackages, false, true,
                         "SHA-256 invalido em " + label);
                return;
            }
        }

        setState("INSTALANDO " + label,
                 ((index * 100) + 97) / totalPackages);

        InstallResult installed =
            installer_.installLocalPackage(path, pkg.name.empty() ? label : pkg.name);

        if (!installed.ok) {
            active_ = false;
            setState("ERRO", index * 100 / totalPackages, false, true,
                     installed.message);
            return;
        }

        // Keep the PKG until a later cleanup pass. AppInstUtil may continue
        // reading it after this call returns on some environments.
        setState(label + " OK", (index + 1) * 100 / totalPackages);
    }

    active_ = false;
    setState("CONCLUIDO", 100, true, false, "");
}

} // namespace ov
