#include "ct_component.hpp"
#include "ct_audio_processor.hpp"
#include "ct_edit_controller.hpp"
#include "ct_unit_description.hpp"
#include "ct_process_context_requirements.hpp"
#include "ct_plug_view.hpp"
#include "ct_timer_handler.hpp"
#include "ct_host.hpp"
#include "ct_host_loop.hpp"
#include "ct_stream.hpp"
#include "ct_events.hpp"
#include "ct_event_conversion.hpp"
#include "ct_component_caches.hpp"
#include "clap_helpers.hpp"
#include "utility/unicode_helpers.hpp"
#include "utility/ct_messages.hpp"
#include "utility/ct_scope.hpp"
#include "libs/span.hpp"
#include <cstring>

namespace ct {

const ct_component::vtable ct_component::s_vtable;

ct_component::ct_component(const v3_tuid clsiid, const clap_plugin_factory *factory, const clap_plugin_descriptor *desc, v3::object *hostcontext, bool *init_ok)
{
    *init_ok = false;

    std::memcpy(m_clsiid, clsiid, sizeof(v3_tuid));

    ct_host *host = new ct_host{hostcontext};
    m_host.reset(host);
    host->m_clap_host.host_data = this;

    const clap_plugin *plug = CLAP_CALL(factory, create_plugin, factory, &host->m_clap_host, desc->id);
    if (!plug)
        return;
    auto plug_cleanup = ct::defer([plug, init_ok]() { if (!*init_ok) CLAP_CALL(plug, destroy, plug); });

    if (!CLAP_CALL(plug, init, plug))
        return;

    m_plug = plug;
    m_desc = desc;

    // interfaces
    m_audio_processor.reset(new ct_audio_processor);
    m_audio_processor->m_comp = this;

    m_edit_controller.reset(new ct_edit_controller);
    m_edit_controller->m_comp = this;

    m_unit_description.reset(new ct_unit_description);
    m_unit_description->m_comp = this;

    m_process_context_requirements.reset(new ct_process_context_requirements);
    m_process_context_requirements->m_comp = this;

    // processor
    m_transport.header.size = sizeof(m_transport);
    m_transport.header.type = CLAP_EVENT_TRANSPORT;
    m_input_events.reset(new ct_events_buffer{ct_events_buffer_capacity});
    m_output_events.reset(new ct_events_buffer{ct_events_buffer_capacity});

    // extensions
    const clap_plugin_audio_ports *audio_ports = (const clap_plugin_audio_ports *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_AUDIO_PORTS);
    if (!audio_ports)
        audio_ports = get_default_clap_plugin_audio_ports();
    m_ext.m_audio_ports = audio_ports;

    const clap_plugin_audio_ports_config *audio_ports_config = (const clap_plugin_audio_ports_config *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_AUDIO_PORTS_CONFIG);
    m_ext.m_audio_ports_config = audio_ports_config;

    const clap_plugin_surround *surround = (const clap_plugin_surround *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_SURROUND);
    m_ext.m_surround = surround;

    const clap_plugin_state *state = (const clap_plugin_state *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_STATE);
    m_ext.m_state = state;

    const clap_plugin_latency *latency = (const clap_plugin_latency *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_LATENCY);
    m_ext.m_latency = latency;

    const clap_plugin_tail *tail = (const clap_plugin_tail *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_TAIL);
    m_ext.m_tail = tail;

    const clap_plugin_render *render = (const clap_plugin_render *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_RENDER);
    m_ext.m_render = render;

    const clap_plugin_params *params = (const clap_plugin_params *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_PARAMS);
    m_ext.m_params = params;

    const clap_plugin_timer_support *timer_support = (const clap_plugin_timer_support *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_TIMER_SUPPORT);
    m_ext.m_timer_support = timer_support;

#if CT_X11
    const clap_plugin_posix_fd_support *posix_fd_support = (const clap_plugin_posix_fd_support *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_POSIX_FD_SUPPORT);
    m_ext.m_posix_fd_support = posix_fd_support;
#endif

    const clap_plugin_gui *gui = (const clap_plugin_gui *)CLAP_CALL(plug, get_extension, plug, CLAP_EXT_GUI);
    m_ext.m_gui = gui;

    // caches
    ct_caches *cache = new ct_caches{this};
    m_cache.reset(cache);
    cache->m_callback_data = this;
    cache->on_cache_update = &on_cache_update;
    cache->update_caches_now();

    // idle timer
    if (host->m_host_loop->register_timer(v3_idle_timer_interval, &m_idle_timer_id, true))
        m_have_idle_timer = true;

    //
    *init_ok = true;
}

ct_component::~ct_component()
{
    if (m_have_idle_timer)
        ct_host::timer_support__unregister_timer(&m_host->m_clap_host, m_idle_timer_id);
    if (ct_plug_view *editor = m_editor)
        editor->m_vptr->i_unk.unref(editor);
    if (const clap_plugin *plug = m_plug)
        CLAP_CALL(plug, destroy, plug);
}

void ct_component::on_reserved_timer(clap_id timer_id)
{
    const clap_plugin *plug = (const clap_plugin *)m_plug;

    if (m_have_idle_timer && m_idle_timer_id == timer_id) {
        if (m_flag_sched_plugin_callback.exchange(0, std::memory_order_relaxed))
            CLAP_CALL(plug, on_main_thread, plug);
    }
}

void ct_component::on_cache_update(void *self_, uint32_t flags)
{
    ct_component *self = (ct_component *)self_;

    if (flags & ct_caches::cache_flags_params)
        self->sync_parameter_values_to_controller_from_plugin();
}

void ct_component::sync_parameter_values_to_controller_from_plugin()
{
    const clap_plugin *plug = m_plug;
    const clap_plugin_params *params = m_ext.m_params;
    const ct_caches::params_t *cache = m_cache->get_params();

    uint32_t count = (uint32_t)cache->m_params.size();
    m_param_value_cache.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        const clap_param_info &info = cache->m_params[i];
        double value = 0;
        if (CLAP_CALL(params, get_value, plug, info.id, &value))
            m_param_value_cache[i] = value;
        else
            m_param_value_cache[i] = 0;
    }
}

#if CT_X11
void ct_component::set_run_loop(v3::run_loop *runloop)
{
    m_host->set_run_loop(runloop);
}
#endif

v3_result V3_API ct_component::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    v3::object *result = nullptr;
    ct_component *self = (ct_component *)self_;

    if (!std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_plugin_base_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_component_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, self->m_clsiid, sizeof(v3_tuid)))
    {
        result = (v3::object *)self_;
    }
    else if (!std::memcmp(iid, v3_audio_processor_iid, sizeof(v3_tuid)))
        result = (v3::object *)self->m_audio_processor.get();
    else if (!std::memcmp(iid, v3_edit_controller_iid, sizeof(v3_tuid)))
        result = (v3::object *)self->m_edit_controller.get();
    else if (!std::memcmp(iid, v3_unit_information_iid, sizeof(v3_tuid)))
        result = (v3::object *)self->m_unit_description.get();
    else if (!std::memcmp(iid, v3_process_context_requirements_iid, sizeof(v3_tuid)))
        result = (v3::object *)self->m_process_context_requirements.get();

    if (result) {
        *obj = result;
        result->m_vptr->i_unk.ref(result);
        LOG_PLUGIN_RET(V3_OK);
    }
    else {
        *obj = nullptr;
        LOG_PLUGIN_RET(V3_NO_INTERFACE);
    }
}

uint32_t V3_API ct_component::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_add(1, std::memory_order_relaxed);
    uint32_t newcnt = oldcnt + 1;

    LOG_PLUGIN_RET(newcnt);
}

uint32_t V3_API ct_component::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_sub(1, std::memory_order_acq_rel);
    uint32_t newcnt = oldcnt - 1;

    if (newcnt == 0)
        delete self;

    LOG_PLUGIN_RET(newcnt);
}

v3_result V3_API ct_component::initialize(void *self_, v3_funknown **context_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    v3::object *context = (v3::object *)context_;

    if (self->m_initialized)
        LOG_PLUGIN_RET(V3_OK);

    self->m_context = context;
    if (context)
        context->m_vptr->i_unk.ref(context);

    self->m_initialized = true;
    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_component::terminate(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (!self->m_initialized)
        LOG_PLUGIN_RET(V3_OK);

    if (v3::object *context = self->m_context) {
        context->m_vptr->i_unk.unref(context);
        self->m_context = nullptr;
    }

    self->m_initialized = false;
    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_component::get_controller_class_id(void *self_, v3_tuid class_id)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    std::memcpy(class_id, self->m_clsiid, sizeof(v3_tuid));

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_component::set_io_mode(void *self_, int32_t io_mode)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)io_mode;
    LOG_PLUGIN_RET(V3_NOT_IMPLEMENTED);
}

int32_t V3_API ct_component::get_bus_count(void *self_, int32_t media_type, int32_t bus_direction)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (media_type == V3_AUDIO) {
        const std::vector<ct_clap_port_info> &ports = self->m_cache->get_audio_ports()->get_port_list(bus_direction == V3_INPUT);
        LOG_PLUGIN_RET(ports.size());
    }
    else if (media_type == V3_EVENT) {
        LOG_PLUGIN_RET(1);
    }
    else {
        LOG_PLUGIN_RET(0);
    }
}

v3_result V3_API ct_component::get_bus_info(void *self_, int32_t media_type, int32_t bus_direction, int32_t bus_idx, struct v3_bus_info *bus_info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    bus_info->media_type = media_type;
    bus_info->direction = bus_direction;

    if (media_type == V3_AUDIO) {
        const std::vector<ct_clap_port_info> &ports = self->m_cache->get_audio_ports()->get_port_list(bus_direction == V3_INPUT);

        if ((uint32_t)bus_idx >= ports.size())
            LOG_PLUGIN_RET(V3_FALSE);

        const clap_audio_port_info ci = ports[(uint32_t)bus_idx];
        bus_info->channel_count = ci.channel_count;
        UTF_copy(bus_info->bus_name, ci.name);
        bus_info->bus_type = (ci.flags & CLAP_AUDIO_PORT_IS_MAIN) ? V3_MAIN : V3_AUX;
        bus_info->flags = V3_DEFAULT_ACTIVE;
        if (!std::strcmp(ci.port_type, CLAP_PORT_CV))
            bus_info->flags |= V3_IS_CONTROL_VOLTAGE;

        LOG_PLUGIN_RET(V3_OK);
    }
    else if (media_type == V3_EVENT) {
        if (bus_idx != 0)
            LOG_PLUGIN_RET(V3_FALSE);

        bus_info->channel_count = 16;
        UTF_copy(bus_info->bus_name, (bus_direction == V3_INPUT) ? "Event input" : "Event output");
        bus_info->bus_type = V3_MAIN;
        bus_info->flags = V3_DEFAULT_ACTIVE;

        LOG_PLUGIN_RET(V3_OK);
    }
    else {
        LOG_PLUGIN_RET(V3_FALSE);
    }
}

v3_result V3_API ct_component::get_routing_info(void *self_, v3_routing_info *input, struct v3_routing_info *output)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)input;
    (void)output;

    LOG_PLUGIN_RET(V3_NOT_IMPLEMENTED);
}

v3_result V3_API ct_component::activate_bus(void *self_, int32_t media_type, int32_t bus_direction, int32_t bus_idx, v3_bool state)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (media_type == V3_AUDIO) {
        const std::vector<ct_clap_port_info> &ports = self->m_cache->get_audio_ports()->get_port_list(bus_direction == V3_INPUT);

        if ((uint32_t)bus_idx >= ports.size())
            LOG_PLUGIN_RET(V3_FALSE);

        // nothing to do
        (void)state;
        LOG_PLUGIN_RET(V3_OK);
    }
    else if (media_type == V3_EVENT) {
        if (bus_idx != 0)
            LOG_PLUGIN_RET(V3_FALSE);

        // nothing to do
        (void)state;
        LOG_PLUGIN_RET(V3_OK);
    }
    else {
        LOG_PLUGIN_RET(V3_FALSE);
    }
}

static void allocate_buffers(ct_component *self, size_t buffer_size)
{
    const ct_caches::ports_t *audio_ports = self->m_cache->get_audio_ports();

    // calculate the required amount of pointer-bump memory
    size_t dynamic_capacity = 0;
    bool can_do_64bit = audio_ports->m_can_do_64bit;
    size_t float_size = can_do_64bit ? sizeof(double) : sizeof(float);

    auto pad_size = [](size_t size) -> size_t {
        constexpr size_t align = alignof(std::max_align_t);
        return (size + (align - 1)) & ~(align - 1);
    };

    for (bool is_input : {false, true}) {
        const std::vector<ct_clap_port_info> &ports = audio_ports->get_port_list(is_input);
        uint32_t num_ports = (uint32_t)ports.size();

        // the array of I/O structures
        dynamic_capacity += pad_size(num_ports * sizeof(clap_audio_buffer));

        for (uint32_t p_idx = 0; p_idx < num_ports; ++p_idx) {
            const ct_clap_port_info &port = ports[p_idx];
            // the channel buffer
            if (is_input)
                dynamic_capacity += port.channel_count * pad_size(buffer_size * float_size);
            // the array of buffer pointers
            dynamic_capacity += pad_size(port.channel_count * sizeof(void *));
        }
    }

    // allocate
    self->m_dynamic_capacity = dynamic_capacity;
    self->m_dynamic_buffers = stdc_allocate<uint8_t>(dynamic_capacity);
    self->m_zero_buffer = stdc_allocate<uint8_t>(buffer_size * float_size);
    self->m_trash_buffer = stdc_allocate<uint8_t>(buffer_size * float_size);
}

static void deallocate_buffers(ct_component *self)
{
    self->m_dynamic_buffers.reset();
    self->m_dynamic_capacity = 0;

    self->m_zero_buffer.reset();
    self->m_trash_buffer.reset();
}

v3_result V3_API ct_component::set_active(void *self_, v3_bool state)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (self->m_active == (bool)state)
        LOG_PLUGIN_RET(V3_OK);

    const clap_plugin *plug = self->m_plug;
    if (state) {
        v3_process_setup setup = self->m_setup;
        if (!CLAP_CALL(plug, activate, plug, setup.sample_rate, 1, (uint32_t)setup.max_block_size))
            LOG_PLUGIN_RET(V3_FALSE);
        //
        allocate_buffers(self, (uint32_t)setup.max_block_size);
        self->m_event_converter_in.reset(new event_converter_v3_to_clap(self));
        self->m_event_converter_out.reset(new event_converter_clap_to_v3(self));
        //
        self->m_active = true;
    }
    else {
        deallocate_buffers(self);
        self->m_event_converter_in.reset();
        self->m_event_converter_out.reset();
        //
        CLAP_CALL(plug, deactivate, plug);
        self->m_active = false;
    }

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_component::set_state(void *self_, v3_bstream **stream_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    const clap_plugin_state *state = self->m_ext.m_state;

    if (!state)
        LOG_PLUGIN_RET(V3_FALSE);

    ct_istream stream{(v3::bstream *)stream_};
    if (!CLAP_CALL(state, load, self->m_plug, &stream.m_istream))
        LOG_PLUGIN_RET(V3_FALSE);

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_component::get_state(void *self_, v3_bstream **stream_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    const clap_plugin_state *state = self->m_ext.m_state;

    if (!state)
        LOG_PLUGIN_RET(V3_FALSE);

    ct_ostream stream{(v3::bstream *)stream_};
    if (!CLAP_CALL(state, save, self->m_plug, &stream.m_ostream))
        LOG_PLUGIN_RET(V3_FALSE);

    LOG_PLUGIN_RET(V3_OK);
}

} // namespace ct
