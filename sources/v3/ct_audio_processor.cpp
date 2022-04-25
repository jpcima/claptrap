#include "ct_audio_processor.hpp"
#include "ct_component.hpp"
#include "clap_helpers.hpp"
#include <cstring>
#include <cassert>

const ct_audio_processor::vtable ct_audio_processor::s_vtable;

v3_result V3_API ct_audio_processor::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    return comp->m_vptr->i_unk.query_interface(comp, iid, obj);
}

uint32_t V3_API ct_audio_processor::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    return comp->m_vptr->i_unk.ref(comp);
}

uint32_t V3_API ct_audio_processor::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    return comp->m_vptr->i_unk.unref(comp);
}

v3_result V3_API ct_audio_processor::set_bus_arrangements(void *self_, v3_speaker_arrangement *inputs, int32_t num_inputs, v3_speaker_arrangement *outputs, int32_t num_outputs)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;

    // check if we have a config that matches
    nonstd::span<const ct_component::ports_config_t> configs;
    uint32_t found = ~uint32_t{0};
    for (uint32_t c_idx = 0; found == ~uint32_t{0} && c_idx < (uint32_t)configs.size(); ++c_idx) {
        const ct_component::ports_config_t &config = configs[c_idx];

        if (config.m_inputs.size() != (uint32_t)num_inputs ||
            config.m_outputs.size() != (uint32_t)num_outputs)
        {
            goto not_found;
        }

        for (uint32_t p_idx = 0; p_idx < (uint32_t)num_inputs; ++p_idx) {
            const clap_audio_port_info_t &port = comp->m_cache.m_audio_inputs[p_idx];
            nonstd::span<const uint8_t> map = comp->m_cache.m_input_channel_maps[p_idx];

            uint64_t arr = 0;
            if (!convert_clap_channels_to_arrangement(port.channel_count, map, &arr) || arr != inputs[p_idx])
                goto not_found;
        }

        for (uint32_t p_idx = 0; p_idx < (uint32_t)num_outputs; ++p_idx) {
            const clap_audio_port_info_t &port = comp->m_cache.m_audio_outputs[p_idx];
            nonstd::span<const uint8_t> map = comp->m_cache.m_output_channel_maps[p_idx];

            uint64_t arr = 0;
            if (!convert_clap_channels_to_arrangement(port.channel_count, map, &arr) || arr != outputs[p_idx])
                goto not_found;
        }

        found = c_idx;

    not_found:
        ;
    }

    if (found == ~uint32_t{0})
        return V3_FALSE;

    // make the matching config active
    const ct_component::ports_config_t &config = configs[found];

    const clap_plugin_audio_ports_config_t *audio_ports_config = comp->m_ext.m_audio_ports_config;
    assert(audio_ports_config);

    if (!audio_ports_config->select(comp->m_plug, config.m_config.id))
        return V3_FALSE;

    // update current ports
    comp->m_cache.m_audio_inputs = config.m_inputs;
    comp->m_cache.m_audio_outputs = config.m_outputs;

    return V3_OK;
}

v3_result V3_API ct_audio_processor::get_bus_arrangement(void *self_, int32_t bus_direction, int32_t idx, v3_speaker_arrangement *speakers)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;

    const std::vector<clap_audio_port_info_t> &ports = (bus_direction == V3_INPUT) ?
        comp->m_cache.m_audio_inputs : comp->m_cache.m_audio_outputs;
    const std::vector<std::vector<uint8_t>> &maps = (bus_direction == V3_INPUT) ?
        comp->m_cache.m_input_channel_maps : comp->m_cache.m_output_channel_maps;

    assert(info.size() == maps.size());

    if ((uint32_t)idx >= ports.size())
        return V3_FALSE;

    if (!convert_clap_channels_to_arrangement(ports[(uint32_t)idx].channel_count, maps[(uint32_t)idx], speakers))
        return V3_FALSE;

    return V3_OK;
}

v3_result V3_API ct_audio_processor::can_process_sample_size(void *self_, int32_t symbolic_sample_size)
{
    LOG_PLUGIN_SELF_CALL(self_);

    if (symbolic_sample_size == V3_SAMPLE_32) {
        return V3_TRUE;
    }
    else if (symbolic_sample_size == V3_SAMPLE_64) {
        //TODO
        return V3_FALSE;
    }
    else {
        return V3_FALSE;
    }
}

uint32_t V3_API ct_audio_processor::get_latency_samples(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_latency_t *latency = comp->m_ext.m_latency;

    if (!latency)
        return 0;

    return latency->get(plug);
}

v3_result V3_API ct_audio_processor::setup_processing(void *self_, v3_process_setup *setup)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;

    if (!can_process_sample_size(self_, setup->symbolic_sample_size))
        return V3_FALSE;

    if (!std::memcmp(&comp->m_setup, setup, sizeof(v3_process_setup)))
        return V3_TRUE;

    std::memcpy(&comp->m_setup, setup, sizeof(v3_process_setup));

    const clap_plugin_render_t *render = comp->m_ext.m_render;
    if (render) {
        clap_plugin_render_mode mode = CLAP_RENDER_REALTIME;
        if (setup->process_mode == V3_OFFLINE)
            mode = CLAP_RENDER_OFFLINE;
        render->set(plug, mode);
    }

    return V3_TRUE;
}

v3_result V3_API ct_audio_processor::set_processing(void *self_, v3_bool state_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;

    bool state = (bool)state_;
    comp->m_should_process = state;

    return V3_TRUE;
}

static void clear_output_buffers(v3_process_data *data)
{
    uint32_t nframes = (uint32_t)data->nframes;
    if (nframes == 0)
        return;

    uint32_t num_bus = (uint32_t)data->num_output_buses;
    for (uint32_t i_bus = 0; i_bus < num_bus; ++i_bus) {
        v3_audio_bus_buffers *bus = &data->outputs[i_bus];
        uint32_t num_ch = bus->num_channels;
        bus->channel_silence_bitset = 0;
        for (uint32_t i_ch = 0; i_ch < num_ch; ++i_ch) {
            if (data->symbolic_sample_size == V3_SAMPLE_64)
                std::memset(bus->channel_buffers_64[i_ch], 0, nframes * sizeof(double));
            else {
                assert(data->symbolic_sample_size == V3_SAMPLE_32);
                std::memset(bus->channel_buffers_32[i_ch], 0, nframes * sizeof(float));
            }
            bus->channel_silence_bitset |= uint64_t{1} << i_ch;
        }
    }
}

v3_result V3_API ct_audio_processor::process(void *self_, v3_process_data *data)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;

    // call `start_processing` and `stop_processing` here
    // there are [audio-thread] in CLAP but more permissive in VST
    if (comp->m_should_process) {
        if (comp->m_processing_status == ct_component::stopped) {
            if (!plug->start_processing(plug))
                comp->m_processing_status = ct_component::errored;
            else {
                plug->reset(plug);
                comp->m_processing_status = ct_component::started;
            }
        }
    }
    else {
        // allow it to clear the errored status
        if (comp->m_processing_status == ct_component::started)
            plug->stop_processing(plug);
        comp->m_processing_status = ct_component::stopped;
    }

    // if not processing, just flush
    if (comp->m_processing_status != ct_component::started) {
        clear_output_buffers(data); 
        //TODO perform flush
        return (comp->m_processing_status != ct_component::errored) ? V3_OK : V3_FALSE;
    }

    #pragma message("TODO: in-place processing")
    #pragma message("TODO: active/inactive buses")
    #pragma message("TODO: parameter flushing")

    //TODO
    return V3_FALSE;
}

uint32_t V3_API ct_audio_processor::get_tail_samples(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_tail_t *tail = comp->m_ext.m_tail;

    if (!tail)
        return 0;

    return tail->get(plug);
}
