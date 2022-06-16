#include "ct_host.hpp"
#include "ct_component.hpp"
#include "ct_event_handler.hpp"
#include "ct_timer_handler.hpp"
#include "unicode_helpers.hpp"
#include "utility/ct_assert.hpp"
#include "utility/ct_messages.hpp"
#include <unordered_map>
#include <cstring>

#if defined(_WIN32)
#   include <windows.h>
#elif defined(__APPLE__)
#   include <CoreFoundation/CoreFoundation.h>
#endif

ct_host::ct_host(v3::object *hostcontext)
{
    // fill in host info
    {
        v3::host_application *app = nullptr;
        if (hostcontext && hostcontext->m_vptr->i_unk.query_interface(hostcontext, v3_host_application_iid, (void **)&app) == V3_OK) {
            v3_str_128 name16{};
            if (app->m_vptr->i_app.get_name(app, name16) == V3_OK) {
                m_clap_host_name = UTF_convert<char>(name16);
                m_clap_host.name = m_clap_host_name.c_str();
            }
        }
    }
}

ct_host::~ct_host()
{
#if CT_X11
    set_run_loop(nullptr);
    // this suffices to clear the posix fds and the timers
#else
    // clear any timers (Windows/macOS: remove them from the native runloop)
    for (clap_id i = 0, n = (clap_id)m_timers.size(); i < n; ++i) {
        timer_data *slot = m_timers[i].get();
        if (slot)
            timer_support__unregister_timer(&m_clap_host, i);
    }
#endif
}

//------------------------------------------------------------------------------
const void *ct_host::get_extension(const clap_host *host, const char *extension_id)
{
    ct_component *comp = (ct_component *)host->host_data;

    if (!std::strcmp(extension_id, CLAP_EXT_LATENCY))
        return &comp->m_host->m_clap_latency;
    if (!std::strcmp(extension_id, CLAP_EXT_PARAMS))
        return &comp->m_host->m_clap_params;
    if (!std::strcmp(extension_id, CLAP_EXT_STATE))
        return &comp->m_host->m_clap_state;
    if (!std::strcmp(extension_id, CLAP_EXT_GUI))
        return &comp->m_host->m_clap_gui;
    if (!std::strcmp(extension_id, CLAP_EXT_TIMER_SUPPORT))
        return &comp->m_host->m_clap_timer_support;
#if CT_X11
    if (!std::strcmp(extension_id, CLAP_EXT_POSIX_FD_SUPPORT))
        return &comp->m_host->m_clap_posix_fd_support;
#endif

    return nullptr;
}

void ct_host::request_restart(const clap_host *host)
{
    //XXX according to clap, plugin should deactivate/reactivate
    // v3 doesn't mention anything special

    ct_component *comp = (ct_component *)host->host_data;
    v3::component_handler *handler = comp->m_handler;

    if (!handler)
        return;

    handler->m_vptr->i_hdr.restart_component(handler, V3_RESTART_RELOAD_COMPONENT);
}

void ct_host::request_process(const clap_host *host)
{
    //XXX nothing that we can do here
    // it can serve as a parameter flush like `request_flush`
    // a v3 host should handle this by itself
    (void)host;
}

void ct_host::request_callback(const clap_host *host)
{
    ct_component *comp = (ct_component *)host->host_data;

    comp->m_flag_sched_plugin_callback.store(1, std::memory_order_relaxed);
}

//------------------------------------------------------------------------------
void ct_host::latency__changed(const clap_host *host)
{
    ct_component *comp = (ct_component *)host->host_data;
    v3::component_handler *handler = comp->m_handler;

    if (!handler)
        return;

    handler->m_vptr->i_hdr.restart_component(handler, V3_RESTART_LATENCY_CHANGED);
}

//------------------------------------------------------------------------------
void ct_host::params__rescan(const clap_host *host, clap_param_rescan_flags flags)
{
    ct_component *comp = (ct_component *)host->host_data;
    v3::component_handler *handler = comp->m_handler;

    if (!handler)
        return;

    int vflags = 0;
    if (flags & CLAP_PARAM_RESCAN_ALL)
        vflags |= V3_RESTART_PARAM_VALUES_CHANGED|V3_RESTART_PARAM_TITLES_CHANGED;
    if (flags & CLAP_PARAM_RESCAN_VALUES)
        vflags |= V3_RESTART_PARAM_VALUES_CHANGED;
    if (flags & (CLAP_PARAM_RESCAN_TEXT|CLAP_PARAM_RESCAN_INFO))
        vflags |= V3_RESTART_PARAM_TITLES_CHANGED;

    if (vflags != 0) {
        // make the controller update its cache of values
        if (vflags & V3_RESTART_PARAM_VALUES_CHANGED)
            comp->sync_parameter_values_to_controller_from_plugin();

        // call the VST3 host as necessary
        handler->m_vptr->i_hdr.restart_component(handler, vflags);
    }
}

void ct_host::params__clear(const clap_host *host, clap_id param_id, clap_param_clear_flags flags)
{
    //XXX probably nothing we can do here
    (void)host;
    (void)param_id;
    (void)flags;
}

void ct_host::params__request_flush(const clap_host *host)
{
    //NOTE: VST3 requires a non-running processor to be called regularly
    // by its host to flush its parameters. This should be unnecessary.
    (void)host;
}

//------------------------------------------------------------------------------
void ct_host::state__mark_dirty(const clap_host *host)
{
    ct_component *comp = (ct_component *)host->host_data;
    v3::component_handler2 *handler2 = comp->m_handler2;

    if (!handler2)
        return;

    handler2->m_vptr->i_hdr.set_dirty(handler2, true);
}

//------------------------------------------------------------------------------
void ct_host::gui__resize_hints_changed(const clap_host_t *host)
{
    //XXX no resize hint support
    (void)host;
}

bool ct_host::gui__request_resize(const clap_host_t *host, uint32_t width, uint32_t height)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_plug_view *view = comp->m_editor;
    v3::plugin_frame *frame = comp->m_editor_frame;

    if (!view || !frame)
        return false;

    v3_view_rect rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;

    return frame->m_vptr->i_frame.resize_view(frame, (v3_plugin_view **)view, &rect) == V3_OK;
}

bool ct_host::gui__request_show(const clap_host_t *host)
{
    //XXX no support
    (void)host;
    return false;
}

bool ct_host::gui__request_hide(const clap_host_t *host)
{
    //XXX no support
    (void)host;
    return false;
}

void ct_host::gui__closed(const clap_host_t *host, bool was_destroyed)
{
    //XXX no support
    (void)host;
    (void)was_destroyed;
}

//------------------------------------------------------------------------------
static void invoke_timer_callback(ct_host::timer_data *timer_data)
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
static std::unordered_map<uintptr_t, ct_host::timer_data *> s_timer_data_by_os_id;

static void CALLBACK os_timer_callback(HWND, UINT, UINT_PTR id, DWORD)
{
    auto it = s_timer_data_by_os_id.find(id);
    if (it != s_timer_data_by_os_id.end())
        invoke_timer_callback(it->second);
}
#endif

bool ct_host::timer_support__register_timer(const clap_host *host, uint32_t period_ms, clap_id *timer_id)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_host *self = comp->m_host.get();

    // allocate a timer slot
    clap_id slot_idx = 0;
    clap_id slot_count = (clap_id)self->m_timers.size();
    while (slot_idx < slot_count && self->m_timers[slot_idx])
        ++slot_idx;
    if (slot_idx == CLAP_INVALID_ID)
        return false;
    if (slot_idx == slot_count) {
        self->m_timers.emplace_back();
        ++slot_count;
    }

    //
    std::unique_ptr<timer_data> slot{new timer_data};
    slot->m_host = self;
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
    if (v3::run_loop *runloop = self->m_runloop) {
        if (runloop->m_vptr->i_loop.register_timer(runloop, (v3_timer_handler **)handler, period_ms) == V3_OK)
            slot->m_status = timer_data::registered;
    }
//------------------------------------------------------------------------------
#else
#   error Unknown platform
#endif

    self->m_timers[slot_idx] = std::move(slot);
    *timer_id = slot_idx;
    return true;
}

bool ct_host::timer_support__unregister_timer(const clap_host *host, clap_id slot_idx)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_host *self = comp->m_host.get();

    if (slot_idx == CLAP_INVALID_ID)
        return false;

    clap_id slot_count = (clap_id)self->m_timers.size();
    if (slot_idx >= slot_count)
        return false;

    timer_data *slot = self->m_timers[slot_idx].get();
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
        v3::run_loop *runloop = self->m_runloop;
        CT_ASSERT(runloop);
        runloop->m_vptr->i_loop.unregister_timer(runloop, (v3_timer_handler **)handler);
    }
    handler->m_vptr->i_unk.unref(handler);
//------------------------------------------------------------------------------
#else
#   error Unknown platform
#endif

    self->m_timers[slot_idx] = nullptr;
    return true;
}

//------------------------------------------------------------------------------
#if CT_X11
static void invoke_posix_fd_callback(ct_host::posix_fd_data *posix_fd_data, int fd, clap_posix_fd_flags_t flags)
{
    ct_component *comp = (ct_component *)posix_fd_data->m_host->m_clap_host.host_data;
    const clap_plugin *plug = comp->m_plug;

    const clap_plugin_posix_fd_support *posix_fd_support = comp->m_ext.m_posix_fd_support;
    if (posix_fd_support)
        CLAP_CALL(posix_fd_support, on_fd, plug, fd, flags);
}

bool ct_host::posix_fd__register_fd(const clap_host *host, int fd, clap_posix_fd_flags_t flags)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_host *self = comp->m_host.get();

    if (flags != CLAP_POSIX_FD_READ)
        return false;

    // check no slot already exists with the same fd
    clap_id slot_idx = 0;
    clap_id slot_count = (clap_id)self->m_posix_fd.size();
    while (slot_idx < slot_count) {
        posix_fd_data *slot = self->m_posix_fd[slot_idx].get();
        if (slot && slot->m_fd == fd)
            return false;
        ++slot_idx;
    }

    // allocate a fd slot
    slot_idx = 0;
    while (slot_idx < slot_count && self->m_posix_fd[slot_idx])
        ++slot_idx;
    if (slot_idx == CLAP_INVALID_ID)
        return false;
    if (slot_idx == slot_count) {
        self->m_posix_fd.emplace_back();
        ++slot_count;
    }

    std::unique_ptr<posix_fd_data> slot{new posix_fd_data};
    slot->m_host = self;
    slot->m_idx = slot_idx;
    slot->m_fd = fd;

    ct_event_handler *handler = new ct_event_handler;
    slot->m_handler = handler;
    handler->m_callback = [](void *data, int fd) {
        posix_fd_data *slot = (posix_fd_data *)data;
        invoke_posix_fd_callback(slot, fd, CLAP_POSIX_FD_READ);
    };
    handler->m_callback_data = slot.get();
    if (v3::run_loop *runloop = self->m_runloop) {
        if (runloop->m_vptr->i_loop.register_event_handler(runloop, (v3_event_handler **)handler, fd) == V3_OK)
            slot->m_status = posix_fd_data::registered;
    }

    self->m_posix_fd[slot_idx] = std::move(slot);
    return true;
}

bool ct_host::posix_fd__modify_fd(const clap_host *host, int fd, clap_posix_fd_flags_t flags)
{
    (void)host;
    (void)fd;
    (void)flags;
    return false;
}

bool ct_host::posix_fd__unregister_fd(const clap_host *host, int fd)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_host *self = comp->m_host.get();

    bool found = false;
    clap_id slot_idx = 0;
    clap_id slot_count = (clap_id)self->m_posix_fd.size();
    while (!found && slot_idx < slot_count) {
        posix_fd_data *slot = self->m_posix_fd[slot_idx].get();
        found = slot && slot->m_fd == fd;
        slot_idx += found ? 0 : 1;
    }

    if (!found)
        return false;

    posix_fd_data *slot = self->m_posix_fd[slot_idx].get();
    if (slot->m_status == posix_fd_data::registered) {
        v3::run_loop *runloop = self->m_runloop;
        CT_ASSERT(runloop);
        runloop->m_vptr->i_loop.unregister_event_handler(runloop, (v3_event_handler **)slot->m_handler);
    }

    self->m_posix_fd[slot_idx] = nullptr;
    return true;
}
#endif

#if CT_X11
void ct_host::set_run_loop(v3::run_loop *runloop)
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
