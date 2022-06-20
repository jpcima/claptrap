#pragma once
#include "utility/ct_safe_fnptr.hpp"
#include "utility/ct_messages.hpp"
#include "utility/ct_scope.hpp"
#include <cstddef>

// A custom UUID used as namespace for VST3 identifiers.
static const unsigned char v3_wrapper_namespace_uuid[16] = {0x18, 0xae, 0xbb, 0x37, 0xf1, 0xa0, 0x40, 0x11, 0xab, 0xe4, 0xc0, 0x8, 0xd6, 0xac, 0xbe, 0x96};


//
enum {
    ct_events_buffer_capacity = 65536,
    ct_port_max_channels = 64,
};

//
enum {
    v3_idle_timer_interval = 50,
};

// Platform definitions
#if !defined (_WIN32) && !defined(__APPLE__)
#   define CT_X11 1
#endif

// Helper to invoke CLAP function pointers safely
// (allows to insert logging if desired)
#define CLAP_CALL(self, member, ...) \
    ::ct::safe_fnptr_access_call((self), +[](decltype(self) x) { return x->member; }, ##__VA_ARGS__)

// Enable to print trace messages
#define VERBOSE_PLUGIN_CALLS 0

#if defined(_MSC_VER)
#   define CT_FUNCSIG __FUNCSIG__
#else
#   define CT_FUNCSIG __PRETTY_FUNCTION__
#endif

#if !VERBOSE_PLUGIN_CALLS
#   define LOG_PLUGIN_CALL
#   define LOG_PLUGIN_SELF_CALL(self)
#   define LOG_PLUGIN_RET(ret) return (ret)
#   define LOG_PLUGIN_RET_PTR(ret) return (ret)
#else
#   define LOG_PLUGIN_CALL                                              \
    ::ct::plugin_call__enter(nullptr, CT_FUNCSIG);                      \
    auto plugin_call__guard = ct::defer([]() { ct::plugin_call__leave(); })

#   define LOG_PLUGIN_SELF_CALL(self)                                   \
    ::ct::plugin_call__enter((self), CT_FUNCSIG);                       \
    auto plugin_call__guard = ct::defer([]() { ct::plugin_call__leave(); })

#   define LOG_PLUGIN_RET(ret) do {                         \
        decltype((ret)) ret__value = (ret);                 \
        ::ct::plugin_log__return().print(ret__value, '\n'); \
        return ret__value;                                  \
    } while (0)

#   define LOG_PLUGIN_RET_PTR(ret) do {                             \
        decltype((ret)) ret__value = (ret);                         \
        ::ct::plugin_log__return().print((void *)ret__value, '\n'); \
        return ret__value;                                          \
    } while (0)

namespace ct {
void plugin_call__enter(void *self, const char *funcsig);
void plugin_call__leave();
message_printer plugin_log__return();
} // namespace ct
#endif
