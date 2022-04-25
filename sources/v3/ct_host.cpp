#include "ct_host.hpp"
#include "ct_component.hpp"
#include "ct_timer_handler.hpp"
#include "unicode_helpers.hpp"
#include <unordered_map>
#include <cstring>
#include <cassert>

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

    // clear any remaining timers
    for (clap_id i = 0, n = (clap_id)m_timers.size(); i < n; ++i) {
        timer_data *slot = m_timers[i].get();
        if (slot)
            timer_support__unregister_timer(&m_clap_host, i);
    }
}

//------------------------------------------------------------------------------
const void *ct_host::get_extension(const clap_host_t *host, const char *extension_id)
{
    ct_component *comp = (ct_component *)host->host_data;

    if (!std::strcmp(extension_id, CLAP_EXT_LATENCY))
        return &comp->m_host->m_clap_latency;
    if (!std::strcmp(extension_id, CLAP_EXT_PARAMS))
        return &comp->m_host->m_clap_params;
    if (!std::strcmp(extension_id, CLAP_EXT_STATE))
        return &comp->m_host->m_clap_state;
    if (!std::strcmp(extension_id, CLAP_EXT_TIMER_SUPPORT))
        return &comp->m_host->m_clap_timer_support;

    return nullptr;
}

void ct_host::request_restart(const clap_host_t *host)
{
    #pragma message("TODO: host/request_restart")
    (void)host;
    LOG_PRINTF("Not implemented: host/request_restart");
}

void ct_host::request_process(const clap_host_t *host)
{
    #pragma message("TODO: host/request_process")
    (void)host;
    LOG_PRINTF("Not implemented: host/request_process");
}

void ct_host::request_callback(const clap_host_t *host)
{
    ct_component *comp = (ct_component *)host->host_data;

    comp->m_flag_sched_plugin_callback.store(1, std::memory_order_relaxed);
}

//------------------------------------------------------------------------------
void ct_host::latency__changed(const clap_host_t *host)
{
    ct_component *comp = (ct_component *)host->host_data;
    v3::component_handler *handler = comp->m_handler;

    if (!handler)
        return;

    handler->m_vptr->i_hdr.restart_component(handler, V3_RESTART_LATENCY_CHANGED);
}

//------------------------------------------------------------------------------
void ct_host::params__rescan(const clap_host_t *host, clap_param_rescan_flags flags)
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

    if (vflags != 0)
        handler->m_vptr->i_hdr.restart_component(handler, vflags);
}

void ct_host::params__clear(const clap_host_t *host, clap_id param_id, clap_param_clear_flags flags)
{
    //TODO
    (void)host;
    (void)param_id;
    (void)flags;
}

void ct_host::params__request_flush(const clap_host_t *host)
{
    #pragma message("TODO host parameter flush")
    (void)host;
}

//------------------------------------------------------------------------------
void ct_host::state__mark_dirty(const clap_host_t *host)
{
    ct_component *comp = (ct_component *)host->host_data;
    v3::component_handler2 *handler2 = comp->m_handler2;

    if (!handler2)
        return;

    handler2->m_vptr->i_hdr.set_dirty(handler2, true);
}

//------------------------------------------------------------------------------
#if defined(_WIN32)
static std::unordered_map<uintptr_t, timer_data *> s_timer_data_by_os_id;
#endif

static void invoke_timer_callback(ct_host::timer_data *timer_data)
{
    ct_component *comp = (ct_component *)timer_data->m_host->m_clap_host.host_data;
    const clap_plugin_t *plug = comp->m_plug;
    clap_id timer_idx = timer_data->m_idx;

    if (timer_data->m_reserved)
        comp->on_reserved_timer(timer_idx);
    else {
        const clap_plugin_timer_support_t *timer_support = comp->m_ext.m_timer_support;
        if (timer_support)
            timer_support->on_timer(plug, timer_idx);
    }
}

bool ct_host::timer_support__register_timer(const clap_host_t *host, uint32_t period_ms, clap_id *timer_id)
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
    auto timer_callback = [](HWND, UINT, UINT_PTR id, DWORD)
    {
        auto it = s_timer_data_by_os_id.find(id);
        if (it != s_timer_data_by_os_id.end())
            invoke_timer_callback(it->second);
    };
    slot->m_timer_id = SetTimer(nullptr, 0, period_ms, +timer_callback);
    if (slot->m_timer_id == 0)
        return false;
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
        // error
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

bool ct_host::timer_support__unregister_timer(const clap_host_t *host, clap_id slot_idx)
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
        assert(runloop);
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

void ct_host::set_run_loop(v3::run_loop *runloop)
{
    v3::run_loop *old = m_runloop;

    if (old == runloop)
        return;

    // unregister from old runloop
    if (old) {
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
