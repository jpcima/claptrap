#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
struct ct_component;

// A subobject of `ct_component`
struct ct_process_context_requirements {
    ct_process_context_requirements() = default;

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static uint32_t V3_API get_process_context_requirements(void *self);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_process_context_requirements i_req {
            &get_process_context_requirements,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    ct_component *m_comp = nullptr;
};
