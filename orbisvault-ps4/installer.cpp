#include "installer.hpp"

#ifdef ORBIS_VAULT_NATIVE_INSTALL
extern "C" {
int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppInstallPkg(const char* file_path, void* reserved);
}
#endif

namespace ov {

bool Installer::initialize() {
#ifdef ORBIS_VAULT_NATIVE_INSTALL
    const int rc = sceAppInstUtilInitialize();
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
    out.message = out.ok ? ("Instalação iniciada: " + displayName)
                         : ("Falha ao iniciar instalação: " + std::to_string(rc));
#else
    out.ok = false;
    out.code = -1;
    out.message = "Native installer adapter not enabled in this build";
#endif
    return out;
}

void Installer::shutdown() {
    // Finalization will be wired after hardware validation of the chosen
    // OpenOrbis/AppInstUtil path.
}

} // namespace ov
