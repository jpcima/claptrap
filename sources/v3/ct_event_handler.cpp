#include "ct_event_handler.hpp"
#include <cstring>

namespace ct {

const ct_event_handler::vtable ct_event_handler::s_vtable;

v3_result V3_API ct_event_handler::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    v3::object *result = nullptr;

    if (!std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_event_handler_iid, sizeof(v3_tuid)))
    {
        result = (v3::object *)self_;
    }

    if (result) {
        *obj = result;
        result->m_vptr->i_unk.ref(result);
        LOG_PLUGIN_RET(V3_OK);
    }
    else {
        *obj = nullptr;
        LOG_PLUGIN_RET(V3_NO_INTERFACE);
    }
}

uint32_t V3_API ct_event_handler::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_event_handler *self = (ct_event_handler *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_add(1, std::memory_order_relaxed);
    uint32_t newcnt = oldcnt + 1;

    LOG_PLUGIN_RET(newcnt);
}

uint32_t V3_API ct_event_handler::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_event_handler *self = (ct_event_handler *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_sub(1, std::memory_order_acq_rel);
    uint32_t newcnt = oldcnt - 1;

    LOG_PLUGIN_RET(newcnt);
}

void V3_API ct_event_handler::on_fd_is_set(void *self_, int fd)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_event_handler *self = (ct_event_handler *)self_;

    if (self->m_callback)
        self->m_callback(self->m_callback_data, fd);
}

} // namespace ct
