#pragma once
#include <cstdio>

// A custom UUID used as namespace for VST3 identifiers.
static const unsigned char v3_wrapper_namespace_uuid[16] = {0x18, 0xae, 0xbb, 0x37, 0xf1, 0xa0, 0x40, 0x11, 0xab, 0xe4, 0xc0, 0x8, 0xd6, 0xac, 0xbe, 0x96};

//
enum {
    v3_idle_timer_interval = 50,
};

// Platform definitions
#if !defined (_WIN32) && !defined(__APPLE__)
#   define CT_X11 1
#endif

// Logging function in printf-style
#define LOG_PRINTF(msg, ...) do {                                   \
        std::fprintf(stderr, "[ct-v3] " msg "\n", ## __VA_ARGS__);  \
        std::fflush(stderr);                                        \
    } while (0)

// Enable to print trace messages
#define VERBOSE_PLUGIN_CALLS 0

#if VERBOSE_PLUGIN_CALLS && defined(__GNUC__)
#   define LOG_PLUGIN_CALL LOG_PRINTF("%s", __PRETTY_FUNCTION__);
#   define LOG_PLUGIN_SELF_CALL(self) LOG_PRINTF("(%p) %s", (self), __PRETTY_FUNCTION__);
#elif VERBOSE_PLUGIN_CALLS && defined(_MSC_VER)
#   define LOG_PLUGIN_CALL LOG_PRINTF("%s", __FUNCSIG__);
#   define LOG_PLUGIN_SELF_CALL(self) LOG_PRINTF("(%p) %s", (self), __FUNCSIG__);
#else
#   define LOG_PLUGIN_CALL
#   define LOG_PLUGIN_SELF_CALL(self)
#endif
