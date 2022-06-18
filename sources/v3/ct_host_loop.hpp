#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <clap/clap.h>
#include <vector>
#include <memory>

namespace ct {

struct ct_event_handler;
struct ct_timer_handler;
struct ct_host;

struct ct_host_loop {
    explicit ct_host_loop(ct_host *host);
    ~ct_host_loop();

    //--------------------------------------------------------------------------
    bool register_timer(uint32_t period_ms, clap_id *timer_id, bool reserved = false);
    bool unregister_timer(clap_id slot_idx);

    //--------------------------------------------------------------------------
#if CT_X11
    bool register_fd(int fd, clap_posix_fd_flags_t flags);
    bool unregister_fd(int fd);
#endif

    //--------------------------------------------------------------------------
#if CT_X11
    void set_run_loop(v3::run_loop *runloop);
#endif

    //--------------------------------------------------------------------------
    ct_host *m_host = nullptr;

    struct timer_data;
    std::vector<std::unique_ptr<timer_data>> m_timers;

#if CT_X11
    struct posix_fd_data;
    std::vector<std::unique_ptr<posix_fd_data>> m_posix_fd;
#endif

#if CT_X11
    v3::run_loop *m_runloop = nullptr;
#endif
};

} // namespace ct
