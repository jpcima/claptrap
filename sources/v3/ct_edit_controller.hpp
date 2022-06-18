#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <travesty/component.h>
#include <travesty/edit_controller.h>

namespace ct {

struct ct_component;

// A subobject of `ct_component`
struct ct_edit_controller {
    ct_edit_controller() = default;

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API initialize(void *self, v3_funknown** context);
    static v3_result V3_API terminate(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API set_component_state(void *self, v3_bstream **stream);
    static v3_result V3_API set_state(void *self, v3_bstream **stream);
    static v3_result V3_API get_state(void *self, v3_bstream **stream);
    static int32_t V3_API get_parameter_count(void *self);
    static v3_result V3_API get_parameter_info(void *self, int32_t param_idx, v3_param_info *info);
    static v3_result V3_API get_parameter_string_for_value(void *self, v3_param_id, double normalised, v3_str_128 output);
    static v3_result V3_API get_parameter_value_for_string(void *self, v3_param_id, int16_t *input, double *output);
    static double V3_API normalised_parameter_to_plain(void *self, v3_param_id, double normalised);
    static double V3_API plain_parameter_to_normalised(void *self, v3_param_id, double plain);
    static double V3_API get_parameter_normalised(void *self, v3_param_id);
    static v3_result V3_API set_parameter_normalised(void *self, v3_param_id, double normalised);
    static v3_result V3_API set_component_handler(void *self, v3_component_handler **handler);
    static v3_plugin_view **V3_API create_view(void *self, const char *name);

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
        const v3_edit_controller i_ctl {
            &set_component_state,
            &set_state,
            &get_state,
            &get_parameter_count,
            &get_parameter_info,
            &get_parameter_string_for_value,
            &get_parameter_value_for_string,
            &normalised_parameter_to_plain,
            &plain_parameter_to_normalised,
            &get_parameter_normalised,
            &set_parameter_normalised,
            &set_component_handler,
            &create_view,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    ct_component *m_comp = nullptr;
};

} // namespace ct
