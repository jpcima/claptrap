#include "ct_component_caches.hpp"
#include "ct_component.hpp"
#include "utility/ct_assert.hpp"
#include "utility/ct_messages.hpp"

namespace ct {

struct ct_caches::impl {
    ct_caches *m_self = nullptr;
    ct_component *m_comp = nullptr;
    uint32_t m_dirty_flags = ~uint32_t{0};
    //
    std::vector<ports_config_t> m_audio_ports_config;
    std::unique_ptr<ports_t> m_audio_ports;
    std::unique_ptr<params_t> m_params;
    //
    void cache_audio_ports(bool do_callback = true);
    void cache_audio_port_configs();
    void cache_params();
};

ct_caches::ct_caches(ct_component *comp)
    : m_priv{new impl}
{
    m_priv->m_self = this;
    m_priv->m_comp = comp;
}

ct_caches::~ct_caches()
{
}

//------------------------------------------------------------------------------
void ct_caches::invalidate_caches(uint32_t flags)
{
    impl *priv = m_priv.get();
    priv->m_dirty_flags |= flags;
}

void ct_caches::update_caches_now()
{
    impl *priv = m_priv.get();
    priv->cache_audio_port_configs();
    priv->cache_audio_ports();
    priv->cache_params();
}

//------------------------------------------------------------------------------
auto ct_caches::get_audio_ports_configs() -> nonstd::span<const ports_config_t>
{
    impl *priv = m_priv.get();
    priv->cache_audio_port_configs();
    return priv->m_audio_ports_config;
}

auto ct_caches::get_audio_ports() -> const ports_t *
{
    impl *priv = m_priv.get();
    priv->cache_audio_ports();
    return priv->m_audio_ports.get();
}

auto ct_caches::get_params() -> const params_t *
{
    impl *priv = m_priv.get();
    priv->cache_params();
    return priv->m_params.get();
}

//------------------------------------------------------------------------------
const std::vector<ct_clap_port_info> &ct_caches::ports_config_t::get_port_list(bool is_input) const
{
    return is_input ? m_inputs : m_outputs;
}

const std::vector<ct_clap_port_info> &ct_caches::ports_t::get_port_list(bool is_input) const
{
    return is_input ? m_inputs : m_outputs;
}

const clap_param_info *ct_caches::params_t::get_param_by_id(clap_id id) const
{
    const uint32_t *idx = m_param_idx_by_id.find(id);
    if (!idx)
        return nullptr;
    return &m_params[*idx];
}

//------------------------------------------------------------------------------
static void cache_audio_ports_internal(
    const clap_plugin *plug, const clap_plugin_audio_ports *audio_ports,
    const clap_plugin_surround *surround,
    bool is_input, std::vector<ct_clap_port_info> &result,
    uint32_t *channel_counter, bool *can_do_64bit)
{
    result.clear();

    uint32_t count = CLAP_CALL(audio_ports, count, plug, is_input);
    result.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        ct_clap_port_info info{};
        if (!CLAP_CALL(audio_ports, get, plug, i, is_input, &info))
            CT_FATAL("Failed to get ", is_input ? "input" : "output", " port: ", i);

        if (channel_counter)
            *channel_counter += info.channel_count;

        uint32_t flags_do_64bit = CLAP_AUDIO_PORT_SUPPORTS_64BITS|CLAP_AUDIO_PORT_REQUIRES_COMMON_SAMPLE_SIZE;
        if ((info.flags & flags_do_64bit) != flags_do_64bit)
            *can_do_64bit = false;

        uint8_t channel_map_buf[ct_port_max_channels] = {};
        uint32_t channel_map_size = 0;
        if (surround)
            channel_map_size = CLAP_CALL(surround, get_channel_map, plug, is_input, i, channel_map_buf, sizeof(ct_port_max_channels));

        nonstd::span<const uint8_t> channel_map{channel_map_buf, channel_map_size};
        info.mapping.configure(info.channel_count, channel_map);

        result.push_back(info);
    }
}

void ct_caches::impl::cache_audio_port_configs()
{
    if ((m_dirty_flags & cache_flags_audio_ports_config) == 0)
        return;

    ct_component *comp = m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_audio_ports_config *audio_ports_config = comp->m_ext.m_audio_ports_config;
    std::vector<ports_config_t> &result = m_audio_ports_config;

    result.clear();

    uint32_t count = 0;
    if (audio_ports_config)
        count = CLAP_CALL(audio_ports_config, count, plug);

    result.reserve(count);

    // go through the configs in order
    for (uint32_t c_idx = 0; c_idx < count; ++c_idx) {
        ports_config_t info{};
        if (!CLAP_CALL(audio_ports_config, get, plug, c_idx, &info.m_config)) {
            CT_WARNING("Could not query audio config ", c_idx);
            continue;
        }

        if (!CLAP_CALL(audio_ports_config, select, plug, info.m_config.id)) {
            CT_WARNING("Could not select audio config ", c_idx);
            continue;
        }

        info.m_total_channels = 0;
        info.m_can_do_64bit = true;
        cache_audio_ports_internal(plug, comp->m_ext.m_audio_ports, comp->m_ext.m_surround, true, info.m_inputs, &info.m_total_channels, &info.m_can_do_64bit);
        cache_audio_ports_internal(plug, comp->m_ext.m_audio_ports, comp->m_ext.m_surround, false, info.m_outputs, &info.m_total_channels, &info.m_can_do_64bit);

        // check the information for consistency
        const uint32_t input_count = info.m_config.input_port_count;
        const uint32_t output_count = info.m_config.output_port_count;
        {
            uint32_t inputs_got = (uint32_t)info.m_inputs.size();
            if (inputs_got != input_count)
                CT_FATAL("Inconsistent inputs from port config: expected ", input_count, ", got ", inputs_got);
        }
        {
            uint32_t outputs_got = (uint32_t)info.m_outputs.size();
            if (outputs_got != output_count)
                CT_FATAL("Inconsistent outputs from port config: expected ", output_count, ", got ", outputs_got);
        }

        // check channel mapping validity
        bool maps_valid = true;
        for (uint32_t p_idx = 0; maps_valid && p_idx < input_count; ++p_idx) {
            maps_valid = info.m_inputs[p_idx].mapping.is_valid();
            if (!maps_valid)
                CT_WARNING("Invalid input mapping ", p_idx, " in port config ", c_idx);
        }
        for (uint32_t p_idx = 0; maps_valid && p_idx < output_count; ++p_idx) {
            maps_valid = info.m_outputs[p_idx].mapping.is_valid();
            if (!maps_valid)
                CT_WARNING("Invalid output mapping ", p_idx, " in port config ", c_idx);
        }

        if (!maps_valid)
            continue; // ignore this port config

        result.push_back(std::move(info));
    }

    // select the first working config we have
    uint32_t other_callback_flags = 0;
    if (audio_ports_config) {
        if (result.empty())
            CT_FATAL("No valid audio config is available");

        ports_config_t &config = result.front();
        if (!CLAP_CALL(audio_ports_config, select, plug, config.m_config.id))
            CT_FATAL("Cannot select the default audio config");

        // update the port cache too
        m_dirty_flags |= cache_flags_audio_ports;
        cache_audio_ports(false); // but don't do its callback yet
        other_callback_flags |= cache_flags_audio_ports;
    }

    m_dirty_flags &= ~cache_flags_audio_ports_config;
    ct::safe_fnptr_call(
        m_self->on_cache_update, m_self->m_callback_data,
        cache_flags_audio_ports_config|other_callback_flags);
}

void ct_caches::impl::cache_audio_ports(bool do_callback)
{
    if ((m_dirty_flags & cache_flags_audio_ports) == 0)
        return;

    ports_t *audio_ports = m_audio_ports.get();
    if (audio_ports)
        *audio_ports = {};
    else {
        audio_ports = new ports_t;
        m_audio_ports.reset(audio_ports);
    }

    ct_component *comp = m_comp;
    const clap_plugin *plug = comp->m_plug;

    CT_ASSERT(comp->m_ext.m_audio_ports);

    audio_ports->m_total_channels = 0;
    audio_ports->m_can_do_64bit = true;
    cache_audio_ports_internal(plug, comp->m_ext.m_audio_ports, comp->m_ext.m_surround, true, audio_ports->m_inputs, &audio_ports->m_total_channels, &audio_ports->m_can_do_64bit);
    cache_audio_ports_internal(plug, comp->m_ext.m_audio_ports, comp->m_ext.m_surround, false, audio_ports->m_outputs, &audio_ports->m_total_channels, &audio_ports->m_can_do_64bit);

    // check our active config, which requires valid mappings
    const uint32_t input_count = (uint32_t)audio_ports->m_inputs.size();
    const uint32_t output_count = (uint32_t)audio_ports->m_outputs.size();
    for (uint32_t p_idx = 0; p_idx < input_count; ++p_idx) {
        if (!audio_ports->m_inputs[p_idx].mapping.is_valid())
            CT_FATAL("Invalid input mapping ", p_idx);
    }
    for (uint32_t p_idx = 0; p_idx < output_count; ++p_idx) {
        if (!audio_ports->m_outputs[p_idx].mapping.is_valid())
            CT_FATAL("Invalid output mapping ", p_idx);
    }

    m_dirty_flags &= ~cache_flags_audio_ports;
    if (do_callback)
        ct::safe_fnptr_call(m_self->on_cache_update, m_self->m_callback_data, cache_flags_audio_ports);
}

void ct_caches::impl::cache_params()
{
    if ((m_dirty_flags & cache_flags_params) == 0)
        return;

    params_t *cache = m_params.get();
    if (cache)
        *cache = {};
    else {
        cache = new params_t;
        m_params.reset(cache);
    }

    ct_component *comp = m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_params *params = comp->m_ext.m_params;
    std::vector<clap_param_info> &result = cache->m_params;
    clap_id_map<uint32_t, 1024> &result_idx_map = cache->m_param_idx_by_id;

    result.clear();
    result_idx_map.clear();

    uint32_t count = 0;
    if (params)
        count = CLAP_CALL(params, count, plug);

    result.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        clap_param_info info{};
        if (!CLAP_CALL(params, get_info, plug, i, &info))
            CT_FATAL("Failed to get parameter: ", i);
        uint32_t idx = (uint32_t)result.size();
        result.push_back(info);
        result_idx_map.set(info.id, idx);
    }

    m_dirty_flags &= ~cache_flags_params;
    ct::safe_fnptr_call(m_self->on_cache_update, m_self->m_callback_data, cache_flags_params);
}

} // namespace ct
