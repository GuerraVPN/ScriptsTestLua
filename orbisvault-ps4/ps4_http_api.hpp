#pragma once

#include <stddef.h>
#include <stdint.h>

// The OpenOrbis Docker image currently ships an older orbis/Http.h where a
// number of known SceHttp exports are declared as void foo().  This local
// compatibility header supplies the ABI signatures used by the official
// net_http sample without modifying the SDK installation.

enum {
    OV_HTTP_METHOD_GET = 0,
    OV_HTTP_VERSION_1_1 = 2,
    OV_HTTP_CONTENTLEN_EXIST = 0
};

extern "C" {

int32_t sceHttpInit(int32_t memId, int32_t sslId, size_t poolSize);
int32_t sceHttpTerm(int32_t httpCtxId);

int32_t sceHttpCreateTemplate(
    int32_t httpCtxId, const char* userAgent, int32_t httpVer, int32_t proxy);
int32_t sceHttpDeleteTemplate(int32_t templateId);

int32_t sceHttpCreateConnectionWithURL(
    int32_t templateId, const char* url, bool isKeepalive);
int32_t sceHttpDeleteConnection(int32_t connId);

int32_t sceHttpCreateRequestWithURL(
    int32_t connId, int32_t method, const char* url, uint64_t contentLength);
int32_t sceHttpCreateRequestWithURL2(
    int32_t connId, const char* method, const char* url, uint64_t contentLength);
int32_t sceHttpDeleteRequest(int32_t reqId);

int32_t sceHttpAddRequestHeader(
    int32_t id, const char* name, const char* value, int32_t mode);

int32_t sceHttpSendRequest(int32_t reqId, const void* postData, size_t size);
int32_t sceHttpGetStatusCode(int32_t reqId, int32_t* statusCode);
int32_t sceHttpGetResponseContentLength(
    int32_t reqId, int32_t* result, size_t* contentLength);
int32_t sceHttpReadData(int32_t reqId, void* data, uint32_t size);

} // extern "C"
