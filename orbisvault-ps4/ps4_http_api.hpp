#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ov {

enum {
    OV_HTTP_METHOD_GET = 0,
    OV_HTTP_VERSION_1_1 = 2,
    OV_HTTP_CONTENTLEN_EXIST = 0
};

struct NativeHttpApi {
    bool loaded = false;

    int32_t (*netInit)(void) = nullptr;
    int32_t (*netPoolCreate)(const char*, int32_t, int32_t) = nullptr;
    void (*netPoolDestroy)(int32_t) = nullptr;

    int32_t (*sslInit)(size_t) = nullptr;
    void (*sslTerm)(int32_t) = nullptr;

    int32_t (*httpInit)(int32_t, int32_t, size_t) = nullptr;
    int32_t (*httpTerm)(int32_t) = nullptr;
    int32_t (*httpCreateTemplate)(int32_t, const char*, int32_t, int32_t) = nullptr;
    int32_t (*httpDeleteTemplate)(int32_t) = nullptr;
    int32_t (*httpCreateConnectionWithURL)(int32_t, const char*, bool) = nullptr;
    int32_t (*httpDeleteConnection)(int32_t) = nullptr;
    int32_t (*httpCreateRequestWithURL)(int32_t, int32_t, const char*, uint64_t) = nullptr;
    int32_t (*httpCreateRequestWithURL2)(int32_t, const char*, const char*, uint64_t) = nullptr;
    int32_t (*httpDeleteRequest)(int32_t) = nullptr;
    int32_t (*httpAddRequestHeader)(int32_t, const char*, const char*, int32_t) = nullptr;
    int32_t (*httpSendRequest)(int32_t, const void*, size_t) = nullptr;
    int32_t (*httpGetStatusCode)(int32_t, int32_t*) = nullptr;
    int32_t (*httpGetResponseContentLength)(int32_t, int32_t*, size_t*) = nullptr;
    int32_t (*httpReadData)(int32_t, void*, uint32_t) = nullptr;
};

NativeHttpApi& nativeHttpApi();

} // namespace ov
