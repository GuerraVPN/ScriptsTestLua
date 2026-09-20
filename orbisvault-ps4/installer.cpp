#include "installer.hpp"

#include <orbis/libkernel.h>
#include <orbis/UserService.h>

#include <stdint.h>

namespace ov {

namespace {

using AppInitFn = int (*)();
using AppTermFn = int (*)();
using AppInstallFn = int (*)(const char*, const char*);
using AppExistsFn = int (*)(const char*, int32_t*);

enum OvLaunchAppFlag : int32_t {
    OV_LAUNCH_NONE = 0,
    OV_LAUNCH_SKIP_SYSTEM_UPDATE = 2
};

struct OvLncAppParam {
    uint32_t size;
    uint32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    OvLaunchAppFlag check_flag;
};

using LaunchFn = int32_t (*)(const char*, const char*[], OvLncAppParam*);

int g_appHandle = -1;
int g_systemHandle = -1;
bool g_appInitialized = false;

AppInitFn g_appInit = nullptr;
AppTermFn g_appTerm = nullptr;
AppInstallFn g_appInstall = nullptr;
AppExistsFn g_appExists = nullptr;
LaunchFn g_launch = nullptr;

template <typename T>
bool resolve(int handle, const char* symbol, T& fn) {
    void* p = nullptr;
    if (handle < 0 || sceKernelDlsym(handle, symbol, &p) != 0 || !p)
        return false;
    fn = reinterpret_cast<T>(p);
    return true;
}

bool ensureAppInstaller() {
    if (g_appInitialized) return true;

    if (g_appHandle < 0) {
        g_appHandle = static_cast<int>(sceKernelLoadStartModule(
            "/system/common/lib/libSceAppInstUtil.sprx",
            0, nullptr, 0, nullptr, nullptr));
        if (g_appHandle < 0) return false;
    }

    if (!g_appInit && !resolve(g_appHandle, "sceAppInstUtilInitialize", g_appInit))
        return false;
    if (!g_appTerm && !resolve(g_appHandle, "sceAppInstUtilTerminate", g_appTerm))
        return false;
    if (!g_appInstall && !resolve(g_appHandle, "sceAppInstUtilAppInstallPkg", g_appInstall))
        return false;
    if (!g_appExists && !resolve(g_appHandle, "sceAppInstUtilAppExists", g_appExists))
        return false;

    const int rc = g_appInit();
    if (rc != 0) return false;

    g_appInitialized = true;
    return true;
}

bool ensureLauncher() {
    if (g_launch) return true;

    g_systemHandle = static_cast<int>(sceKernelLoadStartModule(
        "/system/common/lib/libSceSystemService.sprx",
        0, nullptr, 0, nullptr, nullptr));
    if (g_systemHandle < 0) return false;

    return resolve(g_systemHandle, "sceSystemServiceLaunchApp", g_launch);
}

} // namespace

bool Installer::initialize() {
    // Compatibility build: no privileged module is loaded during boot.
    // Features are resolved only when the user asks for them.
    return true;
}

InstallResult Installer::installLocalPackage(const std::string& pkgPath,
                                             const std::string& displayName) {
    InstallResult out;
    if (!ensureAppInstaller()) {
        out.code = -1;
        out.ok = false;
        out.message = "Instalador do sistema indisponivel neste ambiente";
        return out;
    }

    const int rc = g_appInstall(pkgPath.c_str(), nullptr);
    out.code = rc;
    out.ok = (rc == 0);
    out.message = out.ok ? ("Instalacao iniciada: " + displayName)
                         : ("Falha ao iniciar instalacao: " + std::to_string(rc));
    return out;
}

bool Installer::isInstalled(const std::string& titleId) const {
    if (titleId.empty()) return false;
    if (!ensureAppInstaller()) return false;

    int32_t exists = 0;
    const int rc = g_appExists(titleId.c_str(), &exists);
    return rc == 0 && exists != 0;
}

InstallResult Installer::launchTitle(const std::string& titleId) {
    InstallResult out;
    if (titleId.empty()) {
        out.code = -1;
        out.message = "Title ID vazio";
        return out;
    }
    if (!ensureLauncher()) {
        out.code = -1;
        out.message = "Launcher do sistema indisponivel";
        return out;
    }

    int32_t userId = -1;
    int rc = sceUserServiceGetForegroundUser(&userId);
    if (rc != 0 || userId < 0) {
        out.code = rc;
        out.message = "Nao foi possivel obter o usuario ativo";
        return out;
    }

    OvLncAppParam param{};
    param.size = sizeof(OvLncAppParam);
    param.user_id = static_cast<uint32_t>(userId);
    param.app_opt = 0;
    param.crash_report = 0;
    param.check_flag = OV_LAUNCH_SKIP_SYSTEM_UPDATE;

    const char* argv[] = { nullptr };
    rc = g_launch(titleId.c_str(), argv, &param);

    out.code = rc;
    out.ok = (rc >= 0);
    out.message = out.ok
        ? ("Abrindo " + titleId)
        : ("Falha ao abrir " + titleId + ": " + std::to_string(rc));
    return out;
}

void Installer::shutdown() {
    if (g_appInitialized && g_appTerm) {
        g_appTerm();
        g_appInitialized = false;
    }
}

} // namespace ov
