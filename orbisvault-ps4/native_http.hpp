#pragma once
#include <string>

namespace ov {

bool ensureNativeHttp(std::string* error = nullptr);
int nativeHttpContext();
void shutdownNativeHttp();

} // namespace ov
