#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include "utility/ct_memory.hpp"
#include <travesty/component.h>
#include <travesty/audio_processor.h>
#include <travesty/edit_controller.h>
#include <clap/clap.h>
#include <vector>
#include <atomic>
#include <memory>

namespace ct {

struct ct_audio_processor;
struct ct_edit_controller;
struct ct_unit_description;
struct ct_process_context_requirements;
struct ct_plug_view;
struct ct_timer_handler;
struct ct_host;
class ct_events_buffer;
struct ct_caches;
class event_converter_v3_to_clap;
class event_converter_clap_to_v3;

struct ct_component {
    ct_component(const v3_tuid clsiid, const clap_plugin_factory *factory, const clap_plugin_descriptor *desc, v3::object *hostcontext, bool *init_ok);
    ~ct_component();
    void on_reserved_timer(clap_id timer_id);
    static void on_cache_update(void *self, uint32_t flags);
    void sync_parameter_values_to_controller_from_plugin();
#if CT_X11
    void set_run_loop(v3::run_loop *runloop);
#endif

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API initialize(void *self, v3_funknown **context);
    static v3_result V3_API terminate(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API get_controller_class_id(void *self, v3_tuid class_id);
    static v3_result V3_API set_io_mode(void *self, int32_t io_mode);
    static int32_t V3_API get_bus_count(void *self, int32_t media_type, int32_t bus_direction);
    static v3_result V3_API get_bus_info(void *self, int32_t media_type, int32_t bus_direction, int32_t bus_idx, struct v3_bus_info *bus_info);
    static v3_result V3_API get_routing_info(void *self, v3_routing_info *input, struct v3_routing_info *output);
    static v3_result V3_API activate_bus(void *self, int32_t media_type, int32_t bus_direction, int32_t bus_idx, v3_bool state);
    static v3_result V3_API set_active(void *self, v3_bool state);
    static v3_result V3_API set_state(void *self, v3_bstream **stream);
    static v3_result V3_API get_state(void *self, v3_bstream **stream);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_plugin_base i_plug {
            &initialize,
            &terminate,
        };
        const v3_component i_com {
            &get_controller_class_id,
            &set_io_mode,
            &get_bus_count,
            &get_bus_info,
            &get_routing_info,
            &activate_bus,
            &set_active,
            &set_state,
            &get_state,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    v3_tuid m_clsiid = {}; // dynamic class IID, so each component pretends to be of its own class
    const clap_plugin *m_plug = nullptr;
    const clap_plugin_descriptor *m_desc = nullptr;
    std::unique_ptr<ct_host> m_host;
    std::atomic<unsigned> m_refcnt{1};
    bool m_initialized = false;
    bool m_active = false;
    bool m_should_process = false;
    enum { stopped, started, errored } m_processing_status = stopped;
    v3::object *m_context = nullptr;
    v3::component_handler *m_handler = nullptr;
    v3::component_handler2 *m_handler2 = nullptr;
    v3_process_setup m_setup { V3_REALTIME, V3_SAMPLE_32, 1024, 44100 };

    // subobjects
    std::unique_ptr<ct_audio_processor> m_audio_processor;
    std::unique_ptr<ct_edit_controller> m_edit_controller;
    std::unique_ptr<ct_unit_description> m_unit_description;
    std::unique_ptr<ct_process_context_requirements> m_process_context_requirements;

    // controller
    std::vector<double> m_param_value_cache;

    // processor
    clap_event_transport m_transport{};
    std::unique_ptr<ct_events_buffer> m_input_events;
    std::unique_ptr<ct_events_buffer> m_output_events;

    // events
    std::unique_ptr<event_converter_v3_to_clap> m_event_converter_in;
    std::unique_ptr<event_converter_clap_to_v3> m_event_converter_out;

    // editor
    ct_plug_view *m_editor = nullptr;
    v3::plugin_frame *m_editor_frame = nullptr;
    clap_id m_idle_timer_id = 0;
    bool m_have_idle_timer = false;
#if CT_X11
    v3::run_loop *m_runloop = nullptr;
#endif

    // inter-thread interaction
    std::atomic<int> m_flag_sched_plugin_callback{0};

    // temporary buffers
    stdc_ptr<uint8_t[]> m_dynamic_buffers;
    size_t m_dynamic_capacity = 0;
    stdc_ptr<uint8_t[]> m_zero_buffer;
    stdc_ptr<uint8_t[]> m_trash_buffer;

    // extensions
    struct {
        const clap_plugin_audio_ports *m_audio_ports = nullptr; // required
        const clap_plugin_audio_ports_config *m_audio_ports_config = nullptr; // optional
        const clap_plugin_surround *m_surround = nullptr; // optional
        const clap_plugin_state *m_state = nullptr; // optional
        const clap_plugin_latency *m_latency = nullptr; // optional
        const clap_plugin_tail *m_tail = nullptr; // optional
        const clap_plugin_render *m_render = nullptr; // optional
        const clap_plugin_params *m_params = nullptr; // optional
        const clap_plugin_timer_support *m_timer_support = nullptr; // optional
#if CT_X11
        const clap_plugin_posix_fd_support *m_posix_fd_support = nullptr; // optional
#endif
        const clap_plugin_gui *m_gui = nullptr; // optional
    } m_ext;

    // caches
    std::unique_ptr<ct_caches> m_cache;
};

} // namespace ct
