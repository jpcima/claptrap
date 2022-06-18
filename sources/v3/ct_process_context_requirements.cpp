#include "ct_process_context_requirements.hpp"
#include "ct_component.hpp"

namespace ct {

const ct_process_context_requirements::vtable ct_process_context_requirements::s_vtable;

v3_result V3_API ct_process_context_requirements::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_process_context_requirements *self = (ct_process_context_requirements *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.query_interface(comp, iid, obj));
}

uint32_t V3_API ct_process_context_requirements::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_process_context_requirements *self = (ct_process_context_requirements *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.ref(comp));
}

uint32_t V3_API ct_process_context_requirements::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_process_context_requirements *self = (ct_process_context_requirements *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.unref(comp));
}

uint32_t V3_API ct_process_context_requirements::get_process_context_requirements(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;

    uint32_t req = 0;

    //req |= V3_PROCESS_CTX_NEED_SYSTEM_TIME;
    //req |= V3_PROCESS_CTX_NEED_CONTINUOUS_TIME;
    req |= V3_PROCESS_CTX_NEED_PROJECT_TIME;
    req |= V3_PROCESS_CTX_NEED_BAR_POSITION;
    //req |= V3_PROCESS_CTX_NEED_CYCLE;
    //req |= V3_PROCESS_CTX_NEED_NEXT_CLOCK;
    req |= V3_PROCESS_CTX_NEED_TEMPO;
    req |= V3_PROCESS_CTX_NEED_TIME_SIG;
    //req |= V3_PROCESS_CTX_NEED_CHORD;
    //req |= V3_PROCESS_CTX_NEED_FRAME_RATE;
    req |= V3_PROCESS_CTX_NEED_TRANSPORT_STATE;

    LOG_PLUGIN_RET(req);
}

} // namespace ct
