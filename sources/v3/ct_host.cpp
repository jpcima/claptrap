#include "ct_host.hpp"
#include "ct_host_loop.hpp"
#include "ct_component.hpp"
#include "utility/ct_assert.hpp"
#include "utility/ct_messages.hpp"
#include "utility/unicode_helpers.hpp"
#include <cstring>

namespace ct {

ct_host::ct_host(v3::object *hostcontext)
{
    // fill in host info
    if (hostcontext) {
        v3::host_application *app = nullptr;
        if (hostcontext->m_vptr->i_unk.query_interface(hostcontext, v3_host_application_iid, (void **)&app) == V3_OK) {
            v3_str_128 name16{};
            if (app->m_vptr->i_app.get_name(app, name16) == V3_OK) {
                m_clap_host_name = UTF_convert<char>(name16);
                m_clap_host.name = m_clap_host_name.c_str();
            }
            app->m_vptr->i_unk.unref(app);
        }
    }

    // create host loop
    m_host_loop.reset(new ct_host_loop{this});
}

ct_host::~ct_host()
{
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
bool ct_host::timer_support__register_timer(const clap_host *host, uint32_t period_ms, clap_id *timer_id)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_host *self = comp->m_host.get();
    return self->m_host_loop->register_timer(period_ms, timer_id);
}

bool ct_host::timer_support__unregister_timer(const clap_host *host, clap_id slot_idx)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_host *self = comp->m_host.get();
    return self->m_host_loop->unregister_timer(slot_idx);
}

//------------------------------------------------------------------------------
#if CT_X11
bool ct_host::posix_fd__register_fd(const clap_host *host, int fd, clap_posix_fd_flags_t flags)
{
    ct_component *comp = (ct_component *)host->host_data;
    ct_host *self = comp->m_host.get();
    return self->m_host_loop->register_fd(fd, flags);
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
    return self->m_host_loop->unregister_fd(fd);
}
#endif

#if CT_X11
void ct_host::set_run_loop(v3::run_loop *runloop)
{
    m_host_loop->set_run_loop(runloop);
}
#endif

} // namespace ct
