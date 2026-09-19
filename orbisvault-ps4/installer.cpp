#include "installer.hpp"

#ifdef ORBIS_VAULT_NATIVE_INSTALL
#include <orbis/AppInstUtil.h>
#include <orbis/UserService.h>
#include <stdint.h>

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

extern "C" int32_t sceSystemServiceLaunchApp(
    const char* title_id,
    const char* argv[],
    OvLncAppParam* param);
#endif

namespace ov {

bool Installer::initialize() {
#ifdef ORBIS_VAULT_NATIVE_INSTALL
    const int rc = sceAppInstUtilInitialize();
    // User service can already be initialized by the shell/runtime.
    sceUserServiceInitialize(nullptr);
    return rc == 0;
#else
    return true;
#endif
}

InstallResult Installer::installLocalPackage(const std::string& pkgPath,
                                             const std::string& displayName) {
    InstallResult out;
#ifdef ORBIS_VAULT_NATIVE_INSTALL
    const int rc = sceAppInstUtilAppInstallPkg(pkgPath.c_str(), nullptr);
    out.code = rc;
    out.ok = (rc == 0);
    out.message = out.ok ? ("Instalacao iniciada: " + displayName)
                         : ("Falha ao iniciar instalacao: " + std::to_string(rc));
#else
    out.ok = false;
    out.code = -1;
    out.message = "Native installer adapter not enabled in this build";
#endif
    return out;
}

bool Installer::isInstalled(const std::string& titleId) const {
#ifdef ORBIS_VAULT_NATIVE_INSTALL
    if (titleId.empty()) return false;
    int32_t exists = 0;
    const int rc = sceAppInstUtilAppExists(titleId.c_str(), &exists);
    return rc == 0 && exists != 0;
#else
    (void)titleId;
    return false;
#endif
}

InstallResult Installer::launchTitle(const std::string& titleId) {
    InstallResult out;
#ifdef ORBIS_VAULT_NATIVE_INSTALL
    if (titleId.empty()) {
        out.code = -1;
        out.message = "Title ID vazio";
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
    rc = sceSystemServiceLaunchApp(titleId.c_str(), argv, &param);

    out.code = rc;
    out.ok = (rc >= 0);
    out.message = out.ok
        ? ("Abrindo " + titleId)
        : ("Falha ao abrir " + titleId + ": " + std::to_string(rc));
#else
    out.code = -1;
    out.ok = false;
    out.message = "Native launcher adapter not enabled in this build";
#endif
    return out;
}

void Installer::shutdown() {
#ifdef ORBIS_VAULT_NATIVE_INSTALL
    sceAppInstUtilTerminate();
#endif
}

} // namespace ov
