#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_LINUX)
#if defined(_WIN32) || defined(WIN32)
#define PLATFORM_WIN32
#elif defined(__linux__)
#define PLATFORM_LINUX
#endif
#endif

#if defined(PLATFORM_WIN32)
#include "../../../src/win32/FirstPlatform.h"
#elif defined(PLATFORM_LINUX)
#include "../../../src/linux/FirstPlatform.h"
#else
#error unsupported platform
#endif
