#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <clap/clap.h>
#include <string>
#include <vector>
#include <memory>

struct ct_component;
struct ct_event_handler;
struct ct_timer_handler;

struct ct_host {
    explicit ct_host(v3::object *hostcontext);
    ~ct_host();

    //--------------------------------------------------------------------------
    static const void *get_extension(const clap_host_t *host, const char *extension_id);
    static void request_restart(const clap_host_t *host);
    static void request_process(const clap_host_t *host);
    static void request_callback(const clap_host_t *host);

    //--------------------------------------------------------------------------
    static void latency__changed(const clap_host_t *host);

    //--------------------------------------------------------------------------
    static void params__rescan(const clap_host_t *host, clap_param_rescan_flags flags);
    static void params__clear(const clap_host_t *host, clap_id param_id, clap_param_clear_flags flags);
    static void params__request_flush(const clap_host_t *host);

    //--------------------------------------------------------------------------
    static void state__mark_dirty(const clap_host_t *host);

    //--------------------------------------------------------------------------
    static bool timer_support__register_timer(const clap_host_t *host, uint32_t period_ms, clap_id *timer_id);
    static bool timer_support__unregister_timer(const clap_host_t *host, clap_id timer_id);

    //--------------------------------------------------------------------------
#if CT_X11
    static bool posix_fd__register_fd(const clap_host_t *host, int fd, int flags);
    static bool posix_fd__modify_fd(const clap_host_t *host, int fd, int flags);
    static bool posix_fd__unregister_fd(const clap_host_t *host, int fd);
#endif

    //--------------------------------------------------------------------------
    clap_host m_clap_host {
        CLAP_VERSION,
        nullptr, // ct_component *
        "",
        "",
        "",
        "",
        &get_extension,
        &request_restart,
        &request_process,
        &request_callback,
    };

    std::string m_clap_host_name;

    //--------------------------------------------------------------------------
    const clap_host_latency_t m_clap_latency = {
        &latency__changed,
    };

    //--------------------------------------------------------------------------
    const clap_host_params_t m_clap_params = {
        &params__rescan,
        &params__clear,
        &params__request_flush,
    };

    //--------------------------------------------------------------------------
    const clap_host_state_t m_clap_state = {
        &state__mark_dirty,
    };

    //--------------------------------------------------------------------------
    const clap_host_timer_support_t m_clap_timer_support = {
        &timer_support__register_timer,
        &timer_support__unregister_timer,
    };

#if CT_X11
    const clap_host_posix_fd_support_t m_clap_posix_fd_support = {
        &posix_fd__register_fd,
        &posix_fd__modify_fd,
        &posix_fd__unregister_fd,
     };

    struct posix_fd_data {
        ct_host *m_host = nullptr;
        clap_id m_idx = 0;
        int m_fd = -1;
        enum {
            pending, // timer on wait until runloop available
            registered, // timer operational
        } m_status = pending;
        ct_event_handler *m_handler = nullptr;
    };

    std::vector<std::unique_ptr<posix_fd_data>> m_posix_fd;
#endif

#if CT_X11
    void set_run_loop(v3::run_loop *runloop);
#endif

    struct timer_data {
        ct_host *m_host = nullptr;
        bool m_reserved = false;
        clap_id m_idx = 0;
        uint32_t m_interval = 0;
#if defined(_WIN32)
        uintptr_t m_timer_id = 0;
#elif defined(__APPLE__)
        void *m_timer = nullptr; // CFRunLoopTimerRef
#elif CT_X11
        enum {
            pending, // timer on wait until runloop available
            registered, // timer operational
        } m_status = pending;
        ct_timer_handler *m_handler = nullptr;
#endif
    };

    std::vector<std::unique_ptr<timer_data>> m_timers;
#if CT_X11
    v3::run_loop *m_runloop = nullptr;
#endif
};
