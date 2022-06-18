#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <clap/clap.h>
#include <string>

namespace ct {

struct ct_component;
struct ct_event_handler;
struct ct_timer_handler;
struct ct_host_loop;

struct ct_host {
    explicit ct_host(v3::object *hostcontext);
    ~ct_host();

    //--------------------------------------------------------------------------
    static const void *get_extension(const clap_host *host, const char *extension_id);
    static void request_restart(const clap_host *host);
    static void request_process(const clap_host *host);
    static void request_callback(const clap_host *host);

    //--------------------------------------------------------------------------
    static void latency__changed(const clap_host *host);

    //--------------------------------------------------------------------------
    static void params__rescan(const clap_host *host, clap_param_rescan_flags flags);
    static void params__clear(const clap_host *host, clap_id param_id, clap_param_clear_flags flags);
    static void params__request_flush(const clap_host *host);

    //--------------------------------------------------------------------------
    static void state__mark_dirty(const clap_host *host);

    //--------------------------------------------------------------------------
    static void gui__resize_hints_changed(const clap_host_t *host);
    static bool gui__request_resize(const clap_host_t *host, uint32_t width, uint32_t height);
    static bool gui__request_show(const clap_host_t *host);
    static bool gui__request_hide(const clap_host_t *host);
    static void gui__closed(const clap_host_t *host, bool was_destroyed);

    //--------------------------------------------------------------------------
    static bool timer_support__register_timer(const clap_host *host, uint32_t period_ms, clap_id *timer_id);
    static bool timer_support__unregister_timer(const clap_host *host, clap_id timer_id);

    //--------------------------------------------------------------------------
#if CT_X11
    static bool posix_fd__register_fd(const clap_host *host, int fd, clap_posix_fd_flags_t flags);
    static bool posix_fd__modify_fd(const clap_host *host, int fd, clap_posix_fd_flags_t flags);
    static bool posix_fd__unregister_fd(const clap_host *host, int fd);
#endif

    //--------------------------------------------------------------------------
#if CT_X11
    void set_run_loop(v3::run_loop *runloop);
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
    const clap_host_latency m_clap_latency = {
        &latency__changed,
    };

    //--------------------------------------------------------------------------
    const clap_host_params m_clap_params = {
        &params__rescan,
        &params__clear,
        &params__request_flush,
    };

    //--------------------------------------------------------------------------
    const clap_host_state m_clap_state = {
        &state__mark_dirty,
    };

    //--------------------------------------------------------------------------
    const clap_host_gui m_clap_gui = {
        &gui__resize_hints_changed,
        &gui__request_resize,
        &gui__request_show,
        &gui__request_hide,
        &gui__closed,
    };

    //--------------------------------------------------------------------------
    const clap_host_timer_support m_clap_timer_support = {
        &timer_support__register_timer,
        &timer_support__unregister_timer,
    };

    //--------------------------------------------------------------------------
#if CT_X11
    const clap_host_posix_fd_support m_clap_posix_fd_support = {
        &posix_fd__register_fd,
        &posix_fd__modify_fd,
        &posix_fd__unregister_fd,
     };
#endif

    //--------------------------------------------------------------------------
    std::unique_ptr<ct_host_loop> m_host_loop;
};

} // namespace ct
