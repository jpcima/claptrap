#pragma once

#if defined(__GNUC__)
#define VSTGUI_WARNINGS_PUSH                                    \
    _Pragma("GCC diagnostic push")                              \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-copy\"")     \
    _Pragma("GCC diagnostic ignored \"-Wignored-qualifiers\"")  \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#define VSTGUI_WARNINGS_POP                     \
    _Pragma("GCC diagnostic pop")
#else
#define VSTGUI_WARNINGS_PUSH
#define VSTGUI_WARNINGS_POP
#endif

VSTGUI_WARNINGS_PUSH
#include <vstgui/vstgui.h>
#if defined(_WIN32)
#   include <vstgui/lib/platform/platform_win32.h>
#elif defined(__APPLE__)
#   include <vstgui/lib/platform/platform_macos.h>
#else
#   include <vstgui/lib/platform/platform_x11.h>
#endif
VSTGUI_WARNINGS_POP
