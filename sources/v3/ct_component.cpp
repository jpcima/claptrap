#include "ct_component.hpp"
#include "ct_audio_processor.hpp"
#include "ct_edit_controller.hpp"
#include "ct_unit_description.hpp"
#include "ct_plug_view.hpp"
#include "ct_timer_handler.hpp"
#include "ct_host.hpp"
#include "ct_stream.hpp"
#include "clap_helpers.hpp"
#include "unicode_helpers.hpp"
#include <nonstd/span.hpp>
#include <nonstd/scope.hpp>
#include <cstring>
#include <cassert>

const ct_component::vtable ct_component::s_vtable;

ct_component::ct_component(const v3_tuid clsiid, const clap_plugin_factory_t *factory, const clap_plugin_descriptor_t *desc, v3::object *hostcontext, bool *init_ok)
{
    *init_ok = false;

    std::memcpy(m_clsiid, clsiid, sizeof(v3_tuid));

    ct_host *host = new ct_host{hostcontext};
    m_host.reset(host);
    host->m_clap_host.host_data = this;

    const clap_plugin_t *plug = factory->create_plugin(factory, &host->m_clap_host, desc->id);
    if (!plug)
        return;
    auto plug_cleanup = nonstd::make_scope_exit([plug, init_ok]() { if (!*init_ok) plug->destroy(plug); });

    if (!plug->init(plug))
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

    // extensions
    const clap_plugin_audio_ports_t *audio_ports = (const clap_plugin_audio_ports_t *)plug->get_extension(plug, CLAP_EXT_AUDIO_PORTS);
    if (!audio_ports)
        audio_ports = get_default_clap_plugin_audio_ports();
    m_ext.m_audio_ports = audio_ports;

    const clap_plugin_audio_ports_config_t *audio_ports_config = (const clap_plugin_audio_ports_config_t *)plug->get_extension(plug, CLAP_EXT_AUDIO_PORTS_CONFIG);
    m_ext.m_audio_ports_config = audio_ports_config;

    const clap_plugin_surround_t *surround = (const clap_plugin_surround_t *)plug->get_extension(plug, CLAP_EXT_SURROUND);
    m_ext.m_surround = surround;

    const clap_plugin_state_t *state = (const clap_plugin_state_t *)plug->get_extension(plug, CLAP_EXT_STATE);
    m_ext.m_state = state;

    const clap_plugin_latency_t *latency = (const clap_plugin_latency_t *)plug->get_extension(plug, CLAP_EXT_LATENCY);
    m_ext.m_latency = latency;

    const clap_plugin_tail_t *tail = (const clap_plugin_tail_t *)plug->get_extension(plug, CLAP_EXT_TAIL);
    m_ext.m_tail = tail;

    const clap_plugin_render_t *render = (const clap_plugin_render_t *)plug->get_extension(plug, CLAP_EXT_RENDER);
    m_ext.m_render = render;

    const clap_plugin_params_t *params = (const clap_plugin_params_t *)plug->get_extension(plug, CLAP_EXT_PARAMS);
    m_ext.m_params = params;

    const clap_plugin_timer_support_t *timer_support = (const clap_plugin_timer_support_t *)plug->get_extension(plug, CLAP_EXT_TIMER_SUPPORT);
    m_ext.m_timer_support = timer_support;

    const clap_plugin_gui_t *gui = (const clap_plugin_gui_t *)plug->get_extension(plug, CLAP_EXT_GUI);
    m_ext.m_gui = gui;

    // caches
    cache_audio_port_configs();
    cache_audio_ports();
    cache_channel_maps();
    cache_params();

    // idle timer
    if (ct_host::timer_support__register_timer(&host->m_clap_host, v3_idle_timer_interval, &m_idle_timer_id)) {
        host->m_timers[m_idle_timer_id]->m_reserved = true;
        m_have_idle_timer = true;
    }

    //
    *init_ok = true;
}

ct_component::~ct_component()
{
    if (ct_plug_view *editor = m_editor)
        editor->m_vptr->i_unk.unref(editor);
    if (m_have_idle_timer)
        ct_host::timer_support__unregister_timer(&m_host->m_clap_host, m_idle_timer_id);
    if (const clap_plugin_t *plug = m_plug)
        plug->destroy(plug);
}

void ct_component::on_reserved_timer(clap_id timer_id)
{
    const clap_plugin_t *plug = (const clap_plugin_t *)m_plug;

    if (m_have_idle_timer && m_idle_timer_id == timer_id) {
        if (m_flag_sched_plugin_callback.exchange(0, std::memory_order_relaxed))
            plug->on_main_thread(plug);
    }
}

#if CT_X11
void ct_component::set_run_loop(v3::run_loop *runloop)
{
    m_host->set_run_loop(runloop);

    if (m_runloop == runloop)
        return;

    v3::run_loop *old = m_runloop;
    if (old)
        old->m_vptr->i_unk.unref(old);
    if (runloop)
        runloop->m_vptr->i_unk.ref(runloop);
    m_runloop = runloop;
}
#endif

static void cache_audio_ports_internal(
    const clap_plugin_t *plug, const clap_plugin_audio_ports_t *audio_ports,
    bool is_input, std::vector<clap_audio_port_info_t> &result)
{
    result.clear();

    uint32_t input_count = audio_ports->count(plug, is_input);
    result.reserve(input_count);

    for (uint32_t i = 0; i < input_count; ++i) {
        clap_audio_port_info_t info{};
        if (!audio_ports->get(plug, i, is_input, &info)) {
            // error
            continue;
        }
        result.push_back(info);
    }
}

void ct_component::cache_audio_ports()
{
    assert(m_ext.m_audio_ports);

    cache_audio_ports_internal(m_plug, m_ext.m_audio_ports, true, m_cache.m_audio_inputs);
    cache_audio_ports_internal(m_plug, m_ext.m_audio_ports, false, m_cache.m_audio_outputs);
}

void ct_component::cache_audio_port_configs()
{
    const clap_plugin_t *plug = m_plug;
    const clap_plugin_audio_ports_config_t *audio_ports_config = m_ext.m_audio_ports_config;
    std::vector<ports_config_t> &result = m_cache.m_audio_ports_config;

    result.clear();

    if (!audio_ports_config)
        return;

    uint32_t count = audio_ports_config->count(plug);
    result.reserve(count);

    // do the configs in reverse, so the first valid is the one selected at exit
    for (uint32_t i = count; i-- > 0; ) {
        ports_config_t info{};
        if (!audio_ports_config->get(plug, i, &info.m_config) ||
            !audio_ports_config->select(plug, info.m_config.id))
        {
            // error
            continue;
        }

        cache_audio_ports_internal(plug, m_ext.m_audio_ports, true, info.m_inputs);
        cache_audio_ports_internal(plug, m_ext.m_audio_ports, false, info.m_outputs);

        // check the information for consistency
        if (info.m_inputs.size() != info.m_config.input_port_count ||
            info.m_outputs.size() != info.m_config.output_port_count)
        {
            // error
            continue;
        }

        result.push_back(std::move(info));
    }
}

static void cache_channel_map_internal(
    const clap_plugin_t *plug, const clap_plugin_surround_t *surround,
    nonstd::span<const clap_audio_port_info_t> ports,
    bool is_input, std::vector<std::vector<uint8_t>> &result)
{
    result.clear();
    result.resize(ports.size());

    if (!surround)
        return;

    for (uint32_t i = 0; i < (uint32_t)ports.size(); ++i) {
        uint8_t map[256] = {};
        uint32_t size = surround->get_channel_map(plug, is_input, i, map, sizeof(map));
        result[i].assign(map, map + size);
    }
}

void ct_component::cache_channel_maps()
{
    cache_channel_map_internal(m_plug, m_ext.m_surround, m_cache.m_audio_inputs, true, m_cache.m_input_channel_maps);
    cache_channel_map_internal(m_plug, m_ext.m_surround, m_cache.m_audio_outputs, true, m_cache.m_output_channel_maps);
}

void ct_component::cache_params()
{
    const clap_plugin_t *plug = m_plug;
    const clap_plugin_params_t *params = m_ext.m_params;
    std::vector<clap_param_info_t> &result = m_cache.m_params;
    clap_id_map<uint32_t, 1024> &result_idx_map = m_cache.m_param_idx_by_id;

    result.clear();
    result_idx_map.clear();

    if (!params)
        return;

    uint32_t count = params->count(plug);
    result.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        clap_param_info_t info{};
        if (!params->get_info(plug, i, &info)) {
            // error
            continue;
        }
        uint32_t idx = (uint32_t)result.size();
        result.push_back(info);
        result_idx_map.set(info.id, idx);
    }
}

const clap_param_info_t *ct_component::get_param_by_id(clap_id id) const
{
    const uint32_t *idx = m_cache.m_param_idx_by_id.find(id);
    if (!idx)
        return nullptr;
    return &m_cache.m_params[*idx];
}

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

    if (result) {
        *obj = result;
        result->m_vptr->i_unk.ref(result);
        return V3_OK;
    }
    else {
        *obj = nullptr;
        return V3_NO_INTERFACE;
    }
}

uint32_t V3_API ct_component::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_add(1, std::memory_order_relaxed);
    uint32_t newcnt = oldcnt + 1;

    return newcnt;
}

uint32_t V3_API ct_component::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_sub(1, std::memory_order_acq_rel);
    uint32_t newcnt = oldcnt - 1;

    if (newcnt == 0)
        delete self;

    return newcnt;
}

v3_result V3_API ct_component::initialize(void *self_, v3_funknown **context_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    v3::object *context = (v3::object *)context_;

    if (self->m_initialized)
        return V3_OK;

    self->m_context = context;
    if (context)
        context->m_vptr->i_unk.ref(context);

    self->m_initialized = true;
    return V3_OK;
}

v3_result V3_API ct_component::terminate(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (!self->m_initialized)
        return V3_OK;

    if (v3::object *context = self->m_context) {
        context->m_vptr->i_unk.unref(context);
        self->m_context = nullptr;
    }

    self->m_initialized = false;
    return V3_OK;
}

v3_result V3_API ct_component::get_controller_class_id(void *self_, v3_tuid class_id)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    std::memcpy(class_id, self->m_clsiid, sizeof(v3_tuid));

    return V3_OK;
}

v3_result V3_API ct_component::set_io_mode(void *self_, int32_t io_mode)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)io_mode;
    return V3_NOT_IMPLEMENTED;
}

int32_t V3_API ct_component::get_bus_count(void *self_, int32_t media_type, int32_t bus_direction)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (media_type == V3_AUDIO) {
        const std::vector<clap_audio_port_info_t> &ports = (bus_direction == V3_INPUT) ?
            self->m_cache.m_audio_inputs : self->m_cache.m_audio_outputs;
        return ports.size();
    }
    else if (media_type == V3_EVENT) {
        return 1;
    }
    else {
        return 0;
    }
}

v3_result V3_API ct_component::get_bus_info(void *self_, int32_t media_type, int32_t bus_direction, int32_t bus_idx, struct v3_bus_info *bus_info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    bus_info->media_type = media_type;
    bus_info->direction = bus_direction;

    if (media_type == V3_AUDIO) {
        const std::vector<clap_audio_port_info_t> &ports = (bus_direction == V3_INPUT) ?
            self->m_cache.m_audio_inputs : self->m_cache.m_audio_outputs;

        if ((uint32_t)bus_idx >= ports.size())
            return V3_FALSE;

        const clap_audio_port_info_t ci = ports[(uint32_t)bus_idx];
        bus_info->channel_count = ci.channel_count;
        UTF_copy(bus_info->bus_name, ci.name);
        bus_info->bus_type = (ci.flags & CLAP_AUDIO_PORT_IS_MAIN) ? V3_MAIN : V3_AUX;
        bus_info->flags = V3_DEFAULT_ACTIVE;
        if (!std::strcmp(ci.port_type, CLAP_PORT_CV))
            bus_info->flags |= V3_IS_CONTROL_VOLTAGE;

        return V3_OK;
    }
    else if (media_type == V3_EVENT) {
        if (bus_idx != 0)
            return V3_FALSE;

        bus_info->channel_count = 16;
        UTF_copy(bus_info->bus_name, (bus_direction == V3_INPUT) ? "Event input" : "Event output");
        bus_info->bus_type = V3_MAIN;
        bus_info->flags = V3_DEFAULT_ACTIVE;

        return V3_OK;
    }
    else {
        return V3_FALSE;
    }
}

v3_result V3_API ct_component::get_routing_info(void *self_, v3_routing_info *input, struct v3_routing_info *output)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)input;
    (void)output;

    return V3_NOT_IMPLEMENTED;
}

v3_result V3_API ct_component::activate_bus(void *self_, int32_t media_type, int32_t bus_direction, int32_t bus_idx, v3_bool state)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (media_type == V3_AUDIO) {
        const std::vector<clap_audio_port_info_t> &ports = (bus_direction == V3_INPUT) ?
            self->m_cache.m_audio_inputs : self->m_cache.m_audio_outputs;

        if ((uint32_t)bus_idx >= ports.size())
            return V3_FALSE;

        // nothing to do
        (void)state;
        return V3_OK;
    }
    else if (media_type == V3_EVENT) {
        if (bus_idx != 0)
            return V3_FALSE;

        // nothing to do
        (void)state;
        return V3_OK;
    }
    else {
        return V3_FALSE;
    }
}

v3_result V3_API ct_component::set_active(void *self_, v3_bool state)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;

    if (self->m_active == (bool)state)
        return V3_OK;

    const clap_plugin_t *plug = self->m_plug;
    if (state) {
        if (!plug->activate(plug, self->m_setup.sample_rate, 1, (uint32_t)self->m_setup.max_block_size))
            return V3_FALSE;
        self->m_active = true;
    }
    else {
        plug->deactivate(plug);
        self->m_active = false;
    }

    return V3_OK;
}

v3_result V3_API ct_component::set_state(void *self_, v3_bstream **stream_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    const clap_plugin_state_t *state = self->m_ext.m_state;

    if (!state)
        return V3_FALSE;

    ct_istream stream{(v3::bstream *)stream_};
    if (!state->load(self->m_plug, &stream.m_istream))
        return V3_FALSE;

    return V3_OK;
}

v3_result V3_API ct_component::get_state(void *self_, v3_bstream **stream_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_component *self = (ct_component *)self_;
    const clap_plugin_state_t *state = self->m_ext.m_state;

    if (!state)
        return V3_FALSE;

    ct_ostream stream{(v3::bstream *)stream_};
    if (!state->save(self->m_plug, &stream.m_ostream))
        return V3_FALSE;

    return V3_OK;
}
