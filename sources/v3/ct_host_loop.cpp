#include "ct_host_loop.hpp"
#include "ct_host.hpp"
#include "ct_component.hpp"
#include "ct_event_handler.hpp"
#include "ct_timer_handler.hpp"
#include <unordered_map>

#if defined(_WIN32)
#   include <windows.h>
#elif defined(__APPLE__)
#   include <CoreFoundation/CoreFoundation.h>
#endif

namespace ct {

struct ct_host_loop::timer_data {
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

#if CT_X11
struct ct_host_loop::posix_fd_data {
    ct_host *m_host = nullptr;
    clap_id m_idx = 0;
    int m_fd = -1;
    enum {
        pending, // timer on wait until runloop available
        registered, // timer operational
    } m_status = pending;
    ct_event_handler *m_handler = nullptr;
};
#endif

#if defined(_WIN32)
static std::unordered_map<uintptr_t, ct_host_loop::timer_data *> s_timer_data_by_os_id;
#endif

//------------------------------------------------------------------------------
ct_host_loop::ct_host_loop(ct_host *host)
    : m_host{host}
{
}

ct_host_loop::~ct_host_loop()
{
#if CT_X11
    set_run_loop(nullptr);
    // this suffices to clear the posix fds and the timers
#else
    // clear any timers (Windows/macOS: remove them from the native runloop)
    for (clap_id i = 0, n = (clap_id)m_timers.size(); i < n; ++i) {
        timer_data *slot = m_timers[i].get();
        if (slot) {
#if defined(_WIN32)
            KillTimer(nullptr, slot->m_timer_id);
            s_timer_data_by_os_id.erase(slot->m_timer_id);
#elif defined(__APPLE__)
            CFRunLoopTimerInvalidate((CFRunLoopTimerRef)slot->m_timer);
            CFRelease((CFRunLoopTimerRef)slot->m_timer);
#endif
        }
    }
#endif
}

//------------------------------------------------------------------------------
static void invoke_timer_callback(ct_host_loop::timer_data *timer_data)
{
    ct_component *comp = (ct_component *)timer_data->m_host->m_clap_host.host_data;
    const clap_plugin *plug = comp->m_plug;
    clap_id timer_idx = timer_data->m_idx;

    if (timer_data->m_reserved)
        comp->on_reserved_timer(timer_idx);
    else {
        const clap_plugin_timer_support *timer_support = comp->m_ext.m_timer_support;
        if (timer_support)
            CLAP_CALL(timer_support, on_timer, plug, timer_idx);
    }
}

#if defined(_WIN32)
static void CALLBACK os_timer_callback(HWND, UINT, UINT_PTR id, DWORD)
{
    auto it = s_timer_data_by_os_id.find(id);
    if (it != s_timer_data_by_os_id.end())
        invoke_timer_callback(it->second);
}
#endif

bool ct_host_loop::register_timer(uint32_t period_ms, clap_id *timer_id, bool reserved)
{
    // allocate a timer slot
    clap_id slot_idx = 0;
    clap_id slot_count = (clap_id)m_timers.size();
    while (slot_idx < slot_count && m_timers[slot_idx])
        ++slot_idx;
    if (slot_idx == CLAP_INVALID_ID)
        return false;
    if (slot_idx == slot_count) {
        m_timers.emplace_back();
        ++slot_count;
    }

    //
    std::unique_ptr<timer_data> slot{new timer_data};
    slot->m_host = m_host;
    slot->m_reserved = reserved;
    slot->m_idx = slot_idx;
    slot->m_interval = period_ms;
//------------------------------------------------------------------------------
#if defined(_WIN32)
    slot->m_timer_id = SetTimer(nullptr, 0, period_ms, &os_timer_callback);
    if (slot->m_timer_id == 0) {
        CT_WARNING("Could not register OS timer");
        return false;
    }
    s_timer_data_by_os_id[slot->m_timer_id] = slot.get();
//------------------------------------------------------------------------------
#elif defined(__APPLE__)
    CFRunLoopTimerContext timer_context = {};
    timer_context.info = slot.get();
    auto timer_callback = [](CFRunLoopTimerRef, void *info)
    {
        timer_data *slot = (timer_data *)info;
        invoke_timer_callback(slot);
    };
    slot->m_timer = CFRunLoopTimerCreate(
        kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(),
        1e-3 * ((period_ms > 1) ? period_ms : 1),
        0, 0, +timer_callback, &timer_context);
    if (!slot->m_timer) {
        CT_WARNING("Could not register OS timer");
        return false;
    }
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)slot->m_timer, kCFRunLoopCommonModes);
//------------------------------------------------------------------------------
#elif CT_X11
    ct_timer_handler *handler = new ct_timer_handler;
    slot->m_handler = handler;
    handler->m_callback = [](void *data) {
        timer_data *slot = (timer_data *)data;
        invoke_timer_callback(slot);
    };
    handler->m_callback_data = slot.get();
    if (v3::run_loop *runloop = m_runloop) {
        if (runloop->m_vptr->i_loop.register_timer(runloop, (v3_timer_handler **)handler, period_ms) == V3_OK)
            slot->m_status = timer_data::registered;
    }
//------------------------------------------------------------------------------
#else
#   error Unknown platform
#endif

    m_timers[slot_idx] = std::move(slot);
    *timer_id = slot_idx;
    return true;
}

//------------------------------------------------------------------------------
bool ct_host_loop::unregister_timer(clap_id slot_idx)
{
    if (slot_idx == CLAP_INVALID_ID)
        return false;

    clap_id slot_count = (clap_id)m_timers.size();
    if (slot_idx >= slot_count)
        return false;

    timer_data *slot = m_timers[slot_idx].get();
    if (!slot)
        return false;

//------------------------------------------------------------------------------
#if defined(_WIN32)
    KillTimer(nullptr, slot->m_timer_id);
    s_timer_data_by_os_id.erase(slot->m_timer_id);
//------------------------------------------------------------------------------
#elif defined(__APPLE__)
    CFRunLoopTimerInvalidate((CFRunLoopTimerRef)slot->m_timer);
    CFRelease((CFRunLoopTimerRef)slot->m_timer);
//------------------------------------------------------------------------------
#elif CT_X11
    ct_timer_handler *handler = slot->m_handler;
    if (slot->m_status == timer_data::registered) {
        v3::run_loop *runloop = m_runloop;
        CT_ASSERT(runloop);
        runloop->m_vptr->i_loop.unregister_timer(runloop, (v3_timer_handler **)handler);
    }
    handler->m_vptr->i_unk.unref(handler);
//------------------------------------------------------------------------------
#else
#   error Unknown platform
#endif

    m_timers[slot_idx] = nullptr;
    return true;
}

//------------------------------------------------------------------------------
#if CT_X11
static void invoke_posix_fd_callback(ct_host_loop::posix_fd_data *posix_fd_data, int fd, clap_posix_fd_flags_t flags)
{
    ct_component *comp = (ct_component *)posix_fd_data->m_host->m_clap_host.host_data;
    const clap_plugin *plug = comp->m_plug;

    const clap_plugin_posix_fd_support *posix_fd_support = comp->m_ext.m_posix_fd_support;
    if (posix_fd_support)
        CLAP_CALL(posix_fd_support, on_fd, plug, fd, flags);
}

bool ct_host_loop::register_fd(int fd, clap_posix_fd_flags_t flags)
{
    if (flags != CLAP_POSIX_FD_READ)
        return false;

    // check no slot already exists with the same fd
    clap_id slot_idx = 0;
    clap_id slot_count = (clap_id)m_posix_fd.size();
    while (slot_idx < slot_count) {
        posix_fd_data *slot = m_posix_fd[slot_idx].get();
        if (slot && slot->m_fd == fd)
            return false;
        ++slot_idx;
    }

    // allocate a fd slot
    slot_idx = 0;
    while (slot_idx < slot_count && m_posix_fd[slot_idx])
        ++slot_idx;
    if (slot_idx == CLAP_INVALID_ID)
        return false;
    if (slot_idx == slot_count) {
        m_posix_fd.emplace_back();
        ++slot_count;
    }

    std::unique_ptr<posix_fd_data> slot{new posix_fd_data};
    slot->m_host = m_host;
    slot->m_idx = slot_idx;
    slot->m_fd = fd;

    ct_event_handler *handler = new ct_event_handler;
    slot->m_handler = handler;
    handler->m_callback = [](void *data, int fd) {
        posix_fd_data *slot = (posix_fd_data *)data;
        invoke_posix_fd_callback(slot, fd, CLAP_POSIX_FD_READ);
    };
    handler->m_callback_data = slot.get();
    if (v3::run_loop *runloop = m_runloop) {
        if (runloop->m_vptr->i_loop.register_event_handler(runloop, (v3_event_handler **)handler, fd) == V3_OK)
            slot->m_status = posix_fd_data::registered;
    }

    m_posix_fd[slot_idx] = std::move(slot);
    return true;
}

bool ct_host_loop::unregister_fd(int fd)
{
    bool found = false;
    clap_id slot_idx = 0;
    clap_id slot_count = (clap_id)m_posix_fd.size();
    while (!found && slot_idx < slot_count) {
        posix_fd_data *slot = m_posix_fd[slot_idx].get();
        found = slot && slot->m_fd == fd;
        slot_idx += found ? 0 : 1;
    }

    if (!found)
        return false;

    posix_fd_data *slot = m_posix_fd[slot_idx].get();
    if (slot->m_status == posix_fd_data::registered) {
        v3::run_loop *runloop = m_runloop;
        CT_ASSERT(runloop);
        runloop->m_vptr->i_loop.unregister_event_handler(runloop, (v3_event_handler **)slot->m_handler);
    }

    m_posix_fd[slot_idx] = nullptr;
    return true;
}
#endif

//------------------------------------------------------------------------------
#if CT_X11
void ct_host_loop::set_run_loop(v3::run_loop *runloop)
{
    v3::run_loop *old = m_runloop;

    if (old == runloop)
        return;

    // unregister from old runloop
    if (old) {
        for (clap_id i = 0, n = (clap_id)m_posix_fd.size(); i < n; ++i) {
            posix_fd_data *slot = m_posix_fd[i].get();
            if (slot && slot->m_status == posix_fd_data::registered) {
                old->m_vptr->i_loop.unregister_event_handler(old, (v3_event_handler **)slot->m_handler);
                slot->m_status = posix_fd_data::pending;
            }
        }
        for (clap_id i = 0, n = (clap_id)m_timers.size(); i < n; ++i) {
            timer_data *slot = m_timers[i].get();
            if (slot && slot->m_status == timer_data::registered) {
                old->m_vptr->i_loop.unregister_timer(old, (v3_timer_handler **)slot->m_handler);
                slot->m_status = timer_data::pending;
            }
        }
        old->m_vptr->i_unk.unref(old);
    }

    // register into new runloop
    if (runloop) {
        for (clap_id i = 0, n = (clap_id)m_posix_fd.size(); i < n; ++i) {
            posix_fd_data *slot = m_posix_fd[i].get();
            if (slot && slot->m_status == posix_fd_data::pending) {
                if (runloop->m_vptr->i_loop.register_event_handler(runloop, (v3_event_handler **)slot->m_handler, slot->m_fd) == V3_OK)
                    slot->m_status = posix_fd_data::registered;
            }
        }
        for (clap_id i = 0, n = (clap_id)m_timers.size(); i < n; ++i) {
            timer_data *slot = m_timers[i].get();
            if (slot && slot->m_status == timer_data::pending) {
                if (runloop->m_vptr->i_loop.register_timer(runloop, (v3_timer_handler **)slot->m_handler, slot->m_interval) == V3_OK)
                    slot->m_status = timer_data::registered;
            }
        }
        runloop->m_vptr->i_unk.ref(runloop);
    }

    m_runloop = runloop;
}
#endif

} // namespace ct
