#pragma once
#include <string>

namespace ov {

// Returns lowercase SHA-256 hex for a file. Empty string means failure.
std::string sha256File(const std::string& path);

} // namespace ov
