#pragma once
#include <stdint.h>

namespace ov {

bool ensureNativeHttp();
int nativeHttpContext();
void shutdownNativeHttp();

} // namespace ov
