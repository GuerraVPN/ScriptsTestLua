#include "native_http.hpp"

#include "ps4_http_api.hpp"
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>

namespace ov {

static constexpr int NET_POOL_SIZE = 64 * 1024;

static int g_netPool = 0;
static int g_sslContext = 0;
static int g_httpContext = 0;
static bool g_initialized = false;

bool ensureNativeHttp() {
    if (g_initialized) return true;

    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0)
        return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0)
        return false;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0)
        return false;

    // sceNetInit may already have been called by the environment.
    sceNetInit();

    g_netPool = sceNetPoolCreate("OrbisVaultNet", NET_POOL_SIZE, 0);
    if (g_netPool < 0) {
        g_netPool = 0;
        return false;
    }

    g_sslContext = sceSslInit(128 * 1024U);
    if (g_sslContext < 0) {
        sceNetPoolDestroy(g_netPool);
        g_netPool = 0;
        g_sslContext = 0;
        return false;
    }

    g_httpContext = sceHttpInit(g_netPool, g_sslContext, 128 * 1024U);
    if (g_httpContext < 0) {
        sceSslTerm(g_sslContext);
        sceNetPoolDestroy(g_netPool);
        g_httpContext = g_sslContext = g_netPool = 0;
        return false;
    }

    g_initialized = true;
    return true;
}

int nativeHttpContext() {
    return g_httpContext;
}

void shutdownNativeHttp() {
    if (!g_initialized) return;

    if (g_httpContext > 0) sceHttpTerm(g_httpContext);
    if (g_sslContext > 0) sceSslTerm(g_sslContext);
    if (g_netPool > 0) sceNetPoolDestroy(g_netPool);

    g_httpContext = 0;
    g_sslContext = 0;
    g_netPool = 0;
    g_initialized = false;
}

} // namespace ov
