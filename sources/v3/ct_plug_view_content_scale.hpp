#pragma once
#include "ct_defs.hpp"
#include <travesty/view.h>

namespace ct {

struct ct_plug_view;

// A subobject of `ct_plug_view`
struct ct_plug_view_content_scale {
    ct_plug_view_content_scale() = default;

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API set_content_scale_factor(void *self, float factor);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_plugin_view_content_scale i_scale {
            &set_content_scale_factor,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    ct_plug_view *m_view = nullptr;
};

} // namespace ct
