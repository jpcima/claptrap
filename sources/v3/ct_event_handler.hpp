#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <travesty/view.h>
#include <atomic>

namespace ct {

struct ct_event_handler {
    ct_event_handler() = default;

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static void V3_API on_fd_is_set(void *self, int fd);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_event_handler i_handler {
            &on_fd_is_set,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    std::atomic<unsigned> m_refcnt{1};
    void (*m_callback)(void *, int) = nullptr;
    void *m_callback_data;
};

} // namespace ct
