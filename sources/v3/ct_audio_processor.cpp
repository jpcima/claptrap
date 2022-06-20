#include "ct_audio_processor.hpp"
#include "ct_component.hpp"
#include "ct_events.hpp"
#include "ct_event_conversion.hpp"
#include "ct_component_caches.hpp"
#include "ct_threads.hpp"
#include "clap_helpers.hpp"
#include "utility/ct_bump_allocator.hpp"
#include "utility/ct_messages.hpp"
#include "utility/ct_assert.hpp"
#include <cstring>

namespace ct {

const ct_audio_processor::vtable ct_audio_processor::s_vtable;

v3_result V3_API ct_audio_processor::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.query_interface(comp, iid, obj));
}

uint32_t V3_API ct_audio_processor::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.ref(comp));
}

uint32_t V3_API ct_audio_processor::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.unref(comp));
}

v3_result V3_API ct_audio_processor::set_bus_arrangements(void *self_, v3_speaker_arrangement *inputs, int32_t num_inputs, v3_speaker_arrangement *outputs, int32_t num_outputs)
{
    LOG_PLUGIN_SELF_CALL(self_);

    main_thread_guard mtg;

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;

    // check if we have a config that matches
    nonstd::span<const ct_caches::ports_config_t> configs = comp->m_cache->get_audio_ports_configs();

    uint32_t found = ~uint32_t{0};
    for (uint32_t c_idx = 0; found == ~uint32_t{0} && c_idx < (uint32_t)configs.size(); ++c_idx) {
        const ct_caches::ports_config_t &config = configs[c_idx];

        //
        auto check_matching = [&config]
            (bool is_input, const v3_speaker_arrangement *speakers, uint32_t num_ports) -> bool
        {
            if (num_ports != (uint32_t)config.get_port_list(is_input).size())
                return false;
            for (uint32_t p_idx = 0; p_idx < num_ports; ++p_idx) {
                const ct_clap_port_info &port = config.get_port_list(is_input)[p_idx];
                CT_ASSERT(port.mapping.is_valid());
                if (port.mapping.get_v3_arrangement() != speakers[p_idx])
                    return false;
            }
            return true;
        };

        //
        if (check_matching(true, inputs, (uint32_t)num_inputs) &&
            check_matching(false, outputs, (uint32_t)num_outputs))
        {
            found = c_idx;
        }
    }

    if (found == ~uint32_t{0})
        LOG_PLUGIN_RET(V3_FALSE);

    // make the matching config active
    const ct_caches::ports_config_t &config = configs[found];

    const clap_plugin_audio_ports_config *audio_ports_config = comp->m_ext.m_audio_ports_config;
    CT_ASSERT(audio_ports_config);

    if (!CLAP_CALL(audio_ports_config, select, comp->m_plug, config.m_config.id))
        LOG_PLUGIN_RET(V3_FALSE);

    // update current ports
    ct_caches *cache = comp->m_cache.get();
    cache->invalidate_caches(ct_caches::cache_flags_audio_ports);
    cache->update_caches_now();

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_audio_processor::get_bus_arrangement(void *self_, int32_t bus_direction, int32_t idx, v3_speaker_arrangement *speakers)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;

    const std::vector<ct_clap_port_info> &ports = comp->m_cache->get_audio_ports()->get_port_list(bus_direction == V3_INPUT);

    if ((uint32_t)idx >= ports.size())
        LOG_PLUGIN_RET(V3_FALSE);

    const ct_clap_port_info &port = ports[(uint32_t)idx];
    CT_ASSERT(port.mapping.is_valid());
    *speakers = port.mapping.get_v3_arrangement();

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_audio_processor::can_process_sample_size(void *self_, int32_t symbolic_sample_size)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;

    if (symbolic_sample_size == V3_SAMPLE_32) {
        LOG_PLUGIN_RET(V3_TRUE);
    }
    else if (symbolic_sample_size == V3_SAMPLE_64) {
        return comp->m_cache->get_audio_ports()->m_can_do_64bit;
    }

    LOG_PLUGIN_RET(V3_FALSE);
}

uint32_t V3_API ct_audio_processor::get_latency_samples(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_latency *latency = comp->m_ext.m_latency;

    uint32_t count = 0;
    if (latency)
        count = CLAP_CALL(latency, get, plug);

    LOG_PLUGIN_RET(count);
}

v3_result V3_API ct_audio_processor::setup_processing(void *self_, v3_process_setup *setup)
{
    LOG_PLUGIN_SELF_CALL(self_);

    main_thread_guard mtg;

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin *plug = comp->m_plug;

    if (!can_process_sample_size(self_, setup->symbolic_sample_size))
        LOG_PLUGIN_RET(V3_FALSE);

    if (!std::memcmp(&comp->m_setup, setup, sizeof(v3_process_setup)))
        LOG_PLUGIN_RET(V3_TRUE);

    std::memcpy(&comp->m_setup, setup, sizeof(v3_process_setup));

    const clap_plugin_render *render = comp->m_ext.m_render;
    if (render) {
        clap_plugin_render_mode mode = CLAP_RENDER_REALTIME;
        if (setup->process_mode == V3_OFFLINE)
            mode = CLAP_RENDER_OFFLINE;
        CLAP_CALL(render, set, plug, mode);
    }

    LOG_PLUGIN_RET(V3_TRUE);
}

v3_result V3_API ct_audio_processor::set_processing(void *self_, v3_bool state_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;

    bool state = (bool)state_;
    comp->m_should_process = state;

    LOG_PLUGIN_RET(V3_TRUE);
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
            if (data->symbolic_sample_size == V3_SAMPLE_64) {
                double *dst = bus->channel_buffers_64[i_ch];
                if (dst)
                    std::memset(dst, 0, nframes * sizeof(double));
            }
            else {
                CT_ASSERT(data->symbolic_sample_size == V3_SAMPLE_32);
                float *dst = bus->channel_buffers_32[i_ch];
                if (dst)
                    std::memset(dst, 0, nframes * sizeof(float));
            }
            bus->channel_silence_bitset |= uint64_t{1} << i_ch;
        }
    }
}

///
static void process_parameters_before(ct_audio_processor *self, v3_process_data *data)
{
    ct_component *comp = self->m_comp;

    // transport
    if (const v3_process_context *ctx = data->ctx)
        convert_v3_transport_to_clap(ctx, &comp->m_transport);

    // initialize buffers
    ct_events_buffer *input_events = comp->m_input_events.get();
    input_events->clear();
    ct_events_buffer *output_events = comp->m_output_events.get();
    output_events->clear();

    // buffer input events
    event_converter_v3_to_clap *conv = comp->m_event_converter_in.get();
    conv->set_input((v3::param_changes *)data->input_params, (v3::event_list *)data->input_events);
    conv->set_output(input_events);
    conv->set_sorting_enabled(true);
    conv->transfer();
}

static void process_parameters_after(ct_audio_processor *self, v3_process_data *data)
{
    ct_component *comp = self->m_comp;

    // transmit output events
    event_converter_clap_to_v3 *conv = comp->m_event_converter_out.get();
    conv->set_input(comp->m_output_events.get());
    conv->set_output((v3::param_changes *)data->output_params, (v3::event_list *)data->output_events);
    conv->transfer();
}

///
template <class T> T **&v3_buffer_ptrs(v3_audio_bus_buffers &ab);
template <> inline float **&v3_buffer_ptrs<float>(v3_audio_bus_buffers &ab) { return ab.channel_buffers_32; }
template <> inline double **&v3_buffer_ptrs<double>(v3_audio_bus_buffers &ab) { return ab.channel_buffers_64; }

template <class T> T **v3_buffer_ptrs(const v3_audio_bus_buffers &ab);
template <> inline float **v3_buffer_ptrs<float>(const v3_audio_bus_buffers &ab) { return ab.channel_buffers_32; }
template <> inline double **v3_buffer_ptrs<double>(const v3_audio_bus_buffers &ab) { return ab.channel_buffers_64; }

///
template <class Real>
static void prepare_processing_buffers(ct_component *comp, v3_process_data *data, ct::bump_allocator *allocator, clap_process *clap_data)
{
    const ct_caches::ports_t *audio_ports = comp->m_cache->get_audio_ports();

    uint32_t nframes = (uint32_t)data->nframes;

    // inputs
    uint32_t num_inputs = (uint32_t)audio_ports->m_inputs.size();
    clap_audio_buffer *inputs = allocator->typed_alloc<clap_audio_buffer>(num_inputs);
    CT_ASSERT(inputs);

    for (uint32_t p_idx = 0; p_idx < num_inputs; ++p_idx) {
        clap_audio_buffer port_data{};
        const ct_clap_port_info &port = audio_ports->m_inputs[p_idx];
        uint32_t channel_count = port.channel_count;

        bool have_port = p_idx < (uint32_t)data->num_input_buses;
        const v3_audio_bus_buffers *v3_port = (!have_port) ? nullptr : &data->inputs[p_idx];

        audio_buffer_ptrs<Real>(port_data) = allocator->typed_alloc<Real *>(channel_count);
        CT_ASSERT(audio_buffer_ptrs<Real>(port_data));
        port_data.channel_count = channel_count;

        for (uint32_t c_idx = 0; c_idx < channel_count; ++c_idx) {
            bool have_channel = have_port && c_idx < (uint32_t)v3_port->num_channels;
            uint32_t c_mapped = port.mapping.map_to_clap(c_idx);
            //
            Real *src = (!have_channel) ? nullptr : v3_buffer_ptrs<Real>(*v3_port)[c_idx];
            Real *dst;
            if (!src) have_channel = false;
            //
            if (!have_channel)
                dst = (Real *)comp->m_zero_buffer.get();
            else {
                dst = allocator->typed_alloc<Real>(nframes);
                CT_ASSERT(dst);
                std::memcpy(dst, src, nframes * sizeof(Real));
            }
            audio_buffer_ptrs<Real>(port_data)[c_mapped] = dst;
            //
            bool silent = !have_channel || nframes < 1 ||
                ((v3_port->channel_silence_bitset & ((uint64_t)1 << c_idx)) && (src[0] == 0));
            if (silent)
                port_data.constant_mask |= (uint64_t)1 << c_mapped;
        }

        inputs[p_idx] = port_data;
    }

    clap_data->audio_inputs = inputs;
    clap_data->audio_inputs_count = num_inputs;

    // outputs
    uint32_t num_outputs = (uint32_t)audio_ports->m_outputs.size();
    clap_audio_buffer *outputs = allocator->typed_alloc<clap_audio_buffer>(num_outputs);
    CT_ASSERT(outputs);

    for (uint32_t p_idx = 0; p_idx < num_outputs; ++p_idx) {
        clap_audio_buffer port_data{};
        const ct_clap_port_info &port = audio_ports->m_outputs[p_idx];
        uint32_t channel_count = port.channel_count;

        bool have_port = p_idx < (uint32_t)data->num_output_buses;
        const v3_audio_bus_buffers *v3_port = (!have_port) ? nullptr : &data->outputs[p_idx];

        audio_buffer_ptrs<Real>(port_data) = allocator->typed_alloc<Real *>(channel_count);
        CT_ASSERT(audio_buffer_ptrs<Real>(port_data));
        port_data.channel_count = channel_count;

        for (uint32_t c_idx = 0; c_idx < channel_count; ++c_idx) {
            bool have_channel = have_port && c_idx < (uint32_t)v3_port->num_channels;
            uint32_t c_mapped = port.mapping.map_to_clap(c_idx);
            //
            Real *src = (!have_channel) ? nullptr : v3_buffer_ptrs<Real>(*v3_port)[c_idx];
            Real *dst;
            if (!src) have_channel = false;
            //
            if (!have_channel)
                dst = (Real *)comp->m_trash_buffer.get();
            else
                dst = src;
            audio_buffer_ptrs<Real>(port_data)[c_mapped] = dst;
        }

        outputs[p_idx] = port_data;
    }

    clap_data->audio_outputs = outputs;
    clap_data->audio_outputs_count = num_outputs;
}

template <class Real>
static void release_processing_buffers(ct_component *comp, v3_process_data *data, ct::bump_allocator *allocator, clap_process *clap_data)
{
    // transfer output buffers back to v3

    const ct_caches::ports_t *audio_ports = comp->m_cache->get_audio_ports();

    const std::vector<ct_clap_port_info> &ports = audio_ports->m_outputs;
    uint32_t num_outputs = (uint32_t)ports.size();
    clap_audio_buffer *outputs = clap_data->audio_outputs;

    uint32_t nframes = (uint32_t)data->nframes;

    // outputs
    for (uint32_t p_idx = 0; p_idx < num_outputs; ++p_idx) {
        const clap_audio_buffer &port_data = outputs[p_idx];
        const ct_clap_port_info &port = ports[p_idx];
        uint32_t channel_count = port.channel_count;

        bool have_port = p_idx < (uint32_t)data->num_output_buses;
        v3_audio_bus_buffers *v3_port = (!have_port) ? nullptr : &data->outputs[p_idx];

        v3_port->channel_silence_bitset = 0;

        for (uint32_t c_idx = 0; c_idx < channel_count; ++c_idx) {
            bool have_channel = have_port && c_idx < (uint32_t)v3_port->num_channels;
            uint32_t c_mapped = port.mapping.map_to_clap(c_idx);
            //
            Real *dst = (!have_channel) ? nullptr : v3_buffer_ptrs<Real>(*v3_port)[c_idx];
            //Real *src = audio_buffer_ptrs<Real>(port_data)[c_mapped];
            if (!dst) have_channel = false;
            //
            bool silent = !have_channel || nframes < 1 ||
                ((port_data.constant_mask & ((uint64_t)1 << c_mapped)) && (dst[0] == 0));
            if (silent)
                v3_port->channel_silence_bitset |= (uint64_t)1 << c_idx;
        }
    }
}

///
v3_result V3_API ct_audio_processor::process(void *self_, v3_process_data *data)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_params *params = comp->m_ext.m_params;

    // call `start_processing` and `stop_processing` here
    // there are [audio-thread] in CLAP but more permissive in VST
    if (comp->m_should_process) {
        if (comp->m_processing_status == ct_component::stopped) {
            if (!CLAP_CALL(plug, start_processing, plug))
                comp->m_processing_status = ct_component::errored;
            else {
                CLAP_CALL(plug, reset, plug);
                comp->m_processing_status = ct_component::started;
            }
        }
    }
    else {
        // allow it to clear the errored status
        if (comp->m_processing_status == ct_component::started)
            CLAP_CALL(plug, stop_processing, plug);
        comp->m_processing_status = ct_component::stopped;
    }

    // if not processing, just flush
    if (comp->m_processing_status != ct_component::started) {
        clear_output_buffers(data);
        //
        process_parameters_before(self, data);
        const clap_input_events clap_evts_in = comp->m_input_events->as_clap_input();
        const clap_output_events clap_evts_out = comp->m_output_events->as_clap_output();
        if (params)
            CLAP_CALL(params, flush, plug, &clap_evts_in, &clap_evts_out);
        process_parameters_after(self, data);
        //
        LOG_PLUGIN_RET((comp->m_processing_status != ct_component::errored) ? V3_OK : V3_FALSE);
    }

    //TODO: in-place processing optimizations
    #pragma message("TODO: parameter flushing")

    //
    process_parameters_before(self, data);
    const clap_input_events clap_evts_in = comp->m_input_events->as_clap_input();
    const clap_output_events clap_evts_out = comp->m_output_events->as_clap_output();

    //
    ct::bump_allocator allocator{comp->m_dynamic_buffers.get(), comp->m_dynamic_capacity};

    //
    clap_process clap_data{};
    clap_data.steady_time = -1;
    clap_data.frames_count = (uint32_t)data->nframes;
    clap_data.transport = &comp->m_transport;
    if (data->symbolic_sample_size == V3_SAMPLE_64)
        prepare_processing_buffers<double>(comp, data, &allocator, &clap_data);
    else
        prepare_processing_buffers<float>(comp, data, &allocator, &clap_data);
    clap_data.in_events = &clap_evts_in;
    clap_data.out_events = &clap_evts_out;
    clap_process_status clap_status = CLAP_CALL(plug, process, plug, &clap_data);
    if (data->symbolic_sample_size == V3_SAMPLE_64)
        release_processing_buffers<double>(comp, data, &allocator, &clap_data);
    else
        release_processing_buffers<float>(comp, data, &allocator, &clap_data);
    process_parameters_after(self, data);

    LOG_PLUGIN_RET((clap_status == CLAP_PROCESS_ERROR) ? V3_FALSE : V3_TRUE);
}

uint32_t V3_API ct_audio_processor::get_tail_samples(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_audio_processor *self = (ct_audio_processor *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_tail *tail = comp->m_ext.m_tail;

    uint32_t count = 0;
    if (tail)
        count = CLAP_CALL(tail, get, plug);

    LOG_PLUGIN_RET(count);
}

} // namespace ct
