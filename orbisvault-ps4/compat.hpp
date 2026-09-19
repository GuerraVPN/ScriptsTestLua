#pragma once
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// OpenOrbis libc headers in the current toolchain do not expose the POSIX
// prototype before libc++'s __threading_support header is parsed.
int nanosleep(const struct timespec* req, struct timespec* rem);

#ifdef __cplusplus
}
#endif
