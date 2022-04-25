#include "ct_plug_view_content_scale.hpp"
#include "ct_plug_view.hpp"
#include "ct_component.hpp"
#include "utility/ct_assert.hpp"

const ct_plug_view_content_scale::vtable ct_plug_view_content_scale::s_vtable;

v3_result V3_API ct_plug_view_content_scale::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view_content_scale *self = (ct_plug_view_content_scale *)self_;
    ct_plug_view *view = self->m_view;
    LOG_PLUGIN_RET(view->m_vptr->i_unk.query_interface(view, iid, obj));
}

uint32_t V3_API ct_plug_view_content_scale::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view_content_scale *self = (ct_plug_view_content_scale *)self_;
    ct_plug_view *view = self->m_view;
    LOG_PLUGIN_RET(view->m_vptr->i_unk.ref(view));
}

uint32_t V3_API ct_plug_view_content_scale::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view_content_scale *self = (ct_plug_view_content_scale *)self_;
    ct_plug_view *view = self->m_view;
    LOG_PLUGIN_RET(view->m_vptr->i_unk.unref(view));
}

v3_result V3_API ct_plug_view_content_scale::set_content_scale_factor(void *self_, float factor)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view_content_scale *self = (ct_plug_view_content_scale *)self_;
    ct_plug_view *view = self->m_view;
    ct_component *comp = view->m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_gui *gui = comp->m_ext.m_gui;

    CT_ASSERT(gui);

    if (!CLAP_CALL(gui, set_scale, plug, factor))
        LOG_PLUGIN_RET(V3_FALSE);

    LOG_PLUGIN_RET(V3_OK);
}
