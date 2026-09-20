#include "native_http.hpp"
#include "ps4_http_api.hpp"

#include <orbis/libkernel.h>

namespace ov {

static constexpr int NET_POOL_SIZE = 64 * 1024;
static constexpr size_t SSL_POOL_SIZE = 128 * 1024;
static constexpr size_t HTTP_POOL_SIZE = 128 * 1024;

static int g_netHandle = -1;
static int g_sslHandle = -1;
static int g_httpHandle = -1;

static int g_netPool = 0;
static int g_sslContext = 0;
static int g_httpContext = 0;
static bool g_initialized = false;
static NativeHttpApi g_api;

NativeHttpApi& nativeHttpApi() {
    return g_api;
}

template <typename T>
static bool resolve(int handle, const char* name, T& out) {
    void* ptr = nullptr;
    if (handle < 0 || sceKernelDlsym(handle, name, &ptr) != 0 || !ptr)
        return false;
    out = reinterpret_cast<T>(ptr);
    return true;
}

static int loadModule(const char* path) {
    return static_cast<int>(
        sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, nullptr));
}

static bool loadHttpSymbols(std::string* error) {
    if (g_api.loaded) return true;

    g_netHandle = loadModule("/system/common/lib/libSceNet.sprx");
    if (g_netHandle < 0) {
        if (error) *error = "Falha ao carregar libSceNet";
        return false;
    }

    g_sslHandle = loadModule("/system/common/lib/libSceSsl.sprx");
    if (g_sslHandle < 0) {
        if (error) *error = "Falha ao carregar libSceSsl";
        return false;
    }

    g_httpHandle = loadModule("/system/common/lib/libSceHttp.sprx");
    if (g_httpHandle < 0) {
        if (error) *error = "Falha ao carregar libSceHttp";
        return false;
    }

    bool ok = true;
    ok &= resolve(g_netHandle, "sceNetInit", g_api.netInit);
    ok &= resolve(g_netHandle, "sceNetPoolCreate", g_api.netPoolCreate);
    ok &= resolve(g_netHandle, "sceNetPoolDestroy", g_api.netPoolDestroy);

    ok &= resolve(g_sslHandle, "sceSslInit", g_api.sslInit);
    ok &= resolve(g_sslHandle, "sceSslTerm", g_api.sslTerm);

    ok &= resolve(g_httpHandle, "sceHttpInit", g_api.httpInit);
    ok &= resolve(g_httpHandle, "sceHttpTerm", g_api.httpTerm);
    ok &= resolve(g_httpHandle, "sceHttpCreateTemplate", g_api.httpCreateTemplate);
    ok &= resolve(g_httpHandle, "sceHttpDeleteTemplate", g_api.httpDeleteTemplate);
    ok &= resolve(g_httpHandle, "sceHttpCreateConnectionWithURL", g_api.httpCreateConnectionWithURL);
    ok &= resolve(g_httpHandle, "sceHttpDeleteConnection", g_api.httpDeleteConnection);
    ok &= resolve(g_httpHandle, "sceHttpCreateRequestWithURL", g_api.httpCreateRequestWithURL);
    ok &= resolve(g_httpHandle, "sceHttpCreateRequestWithURL2", g_api.httpCreateRequestWithURL2);
    ok &= resolve(g_httpHandle, "sceHttpDeleteRequest", g_api.httpDeleteRequest);
    ok &= resolve(g_httpHandle, "sceHttpAddRequestHeader", g_api.httpAddRequestHeader);
    ok &= resolve(g_httpHandle, "sceHttpSendRequest", g_api.httpSendRequest);
    ok &= resolve(g_httpHandle, "sceHttpGetStatusCode", g_api.httpGetStatusCode);
    ok &= resolve(g_httpHandle, "sceHttpGetResponseContentLength", g_api.httpGetResponseContentLength);
    ok &= resolve(g_httpHandle, "sceHttpReadData", g_api.httpReadData);

    if (!ok) {
        if (error) *error = "Uma API de rede nao existe neste firmware";
        return false;
    }

    g_api.loaded = true;
    return true;
}

bool ensureNativeHttp(std::string* error) {
    if (g_initialized) return true;
    if (!loadHttpSymbols(error)) return false;

    // Some environments already initialized networking. Ignore that return.
    g_api.netInit();

    g_netPool = g_api.netPoolCreate("OrbisVaultSafe", NET_POOL_SIZE, 0);
    if (g_netPool < 0) {
        if (error) *error = "Falha ao criar pool de rede";
        g_netPool = 0;
        return false;
    }

    g_sslContext = g_api.sslInit(SSL_POOL_SIZE);
    if (g_sslContext < 0) {
        if (error) *error = "Falha ao iniciar SSL";
        g_api.netPoolDestroy(g_netPool);
        g_netPool = 0;
        return false;
    }

    g_httpContext = g_api.httpInit(g_netPool, g_sslContext, HTTP_POOL_SIZE);
    if (g_httpContext < 0) {
        if (error) *error = "Falha ao iniciar HTTP";
        g_api.sslTerm(g_sslContext);
        g_api.netPoolDestroy(g_netPool);
        g_sslContext = g_netPool = 0;
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

    if (g_api.httpTerm && g_httpContext > 0) g_api.httpTerm(g_httpContext);
    if (g_api.sslTerm && g_sslContext > 0) g_api.sslTerm(g_sslContext);
    if (g_api.netPoolDestroy && g_netPool > 0) g_api.netPoolDestroy(g_netPool);

    g_httpContext = 0;
    g_sslContext = 0;
    g_netPool = 0;
    g_initialized = false;
}

} // namespace ov
