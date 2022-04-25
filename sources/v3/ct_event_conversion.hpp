#pragma once
#include "ct_component_caches.hpp"
#include "travesty_helpers.hpp"
#include <vector>
#include <cstdint>
struct ct_component;
class ct_events_buffer;
struct clap_event_transport;

//------------------------------------------------------------------------------
class event_converter_v3_to_clap {
public:
    explicit event_converter_v3_to_clap(ct_component *comp);
    void set_input(v3::param_changes *pcs, v3::event_list *evs) { m_pcs = pcs; m_evs = evs; }
    void set_output(ct_events_buffer *out) { m_out = out; }
    void set_sorting_enabled(bool en) { m_sort = en; }
    void transfer();

private:
    static uint32_t fix_offset(int32_t offset);
    static void convert_event(const v3_event *event, ct_events_buffer *result);
    static void convert_parameter_change(uint32_t id, int32_t offset, double value, const ct_caches::params_t *clap_params, ct_events_buffer *result);

private:
    v3::param_changes *m_pcs = nullptr;
    v3::event_list *m_evs = nullptr;
    ct_events_buffer *m_out = nullptr;
    bool m_sort = true;
    const ct_caches::params_t *m_cache = nullptr;
};

//------------------------------------------------------------------------------
class event_converter_clap_to_v3 {
public:
    explicit event_converter_clap_to_v3(ct_component *comp);
    void set_input(ct_events_buffer *in) { m_in = in; }
    void set_output(v3::param_changes *pcs, v3::event_list *evs) { m_pcs = pcs; m_evs = evs; }
    void transfer();

private:
    const ct_events_buffer *m_in = nullptr;
    v3::param_changes *m_pcs = nullptr;
    v3::event_list *m_evs = nullptr;
    const ct_caches::params_t *m_cache = nullptr;
    std::vector<v3::param_value_queue *> m_queues;
};

//------------------------------------------------------------------------------
void convert_v3_transport_to_clap(const v3_process_context *ctx, clap_event_transport *transport);
