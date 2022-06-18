#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <travesty/view.h>
#include <atomic>

namespace ct {

struct ct_timer_handler {
    ct_timer_handler() = default;

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static void V3_API on_timer(void *self);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_timer_handler i_handler {
            &on_timer,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    std::atomic<unsigned> m_refcnt{1};
    void (*m_callback)(void *) = nullptr;
    void *m_callback_data;
};

} // namespace ct
