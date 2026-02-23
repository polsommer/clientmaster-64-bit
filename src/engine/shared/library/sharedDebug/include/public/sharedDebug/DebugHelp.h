#if defined(PLATFORM_WIN32)
#if defined(__has_include)
#if __has_include("../../../src/win32/DebugHelp.h")
#include "../../../src/win32/DebugHelp.h"
#elif __has_include(<sharedDebug/src/win32/DebugHelp.h>)
#include <sharedDebug/src/win32/DebugHelp.h>
#elif __has_include(<sharedDebug/win32/DebugHelp.h>)
#include <sharedDebug/win32/DebugHelp.h>
#else
#error unsupported platform
#endif
#else
#include "../../../src/win32/DebugHelp.h"
#endif
#elif defined(PLATFORM_LINUX)
#if defined(__has_include)
#if __has_include("../../../src/linux/DebugHelp.h")
#include "../../../src/linux/DebugHelp.h"
#elif __has_include(<sharedDebug/src/linux/DebugHelp.h>)
#include <sharedDebug/src/linux/DebugHelp.h>
#elif __has_include(<sharedDebug/linux/DebugHelp.h>)
#include <sharedDebug/linux/DebugHelp.h>
#else
#error unsupported platform
#endif
#else
#include "../../../src/linux/DebugHelp.h"
#endif
#else
#error unsupported platform
#endif
