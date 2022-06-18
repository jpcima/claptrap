#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <travesty/component.h>
#include <travesty/audio_processor.h>

namespace ct {

struct ct_component;

// A subobject of `ct_component`
struct ct_audio_processor {
    ct_audio_processor() = default;

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API set_bus_arrangements(void *self, v3_speaker_arrangement *inputs, int32_t num_inputs, v3_speaker_arrangement *outputs, int32_t num_outputs);
    static v3_result V3_API get_bus_arrangement(void *self, int32_t bus_direction, int32_t idx, v3_speaker_arrangement *speakers);
    static v3_result V3_API can_process_sample_size(void *self, int32_t symbolic_sample_size);
    static uint32_t V3_API get_latency_samples(void *self);
    static v3_result V3_API setup_processing(void *self, v3_process_setup *setup);
    static v3_result V3_API set_processing(void *self, v3_bool state);
    static v3_result V3_API process(void *self, v3_process_data *data);
    static uint32_t V3_API get_tail_samples(void *self);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_audio_processor i_aud {
            &set_bus_arrangements,
            &get_bus_arrangement,
            &can_process_sample_size,
            &get_latency_samples,
            &setup_processing,
            &set_processing,
            &process,
            &get_tail_samples,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    ct_component *m_comp = nullptr;
};

} // namespace ct
