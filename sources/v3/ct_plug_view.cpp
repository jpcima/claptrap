#include "ct_plug_view.hpp"
#include "ct_plug_view_content_scale.hpp"
#include "ct_component.hpp"
#include "clap_helpers.hpp"
#include <algorithm>
#include <cstring>
#include <cassert>

const ct_plug_view::vtable ct_plug_view::s_vtable;

ct_plug_view::ct_plug_view()
{
    m_content_scale.reset(new ct_plug_view_content_scale);
    m_content_scale->m_view = this;
}

ct_plug_view::~ct_plug_view()
{
}

v3_result V3_API ct_plug_view::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    v3::object *result = nullptr;
    ct_plug_view *self = (ct_plug_view *)self_;

    if (!std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_plugin_view_iid, sizeof(v3_tuid)))
    {
        result = (v3::object *)self_;
    }
    else if (!std::memcmp(iid, v3_plugin_view_content_scale_iid, sizeof(v3_tuid)))
    {
        result = (v3::object *)self->m_content_scale.get();
    }

    if (result) {
        *obj = result;
        result->m_vptr->i_unk.ref(result);
        return V3_OK;
    }
    else {
        *obj = nullptr;
        return V3_NO_INTERFACE;
    }
}

uint32_t V3_API ct_plug_view::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_add(1, std::memory_order_relaxed);
    uint32_t newcnt = oldcnt + 1;

    return newcnt;
}

uint32_t V3_API ct_plug_view::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_sub(1, std::memory_order_acq_rel);
    uint32_t newcnt = oldcnt - 1;

    if (newcnt == 0) {
        if (self->m_delete_hook)
            self->m_delete_hook(self);
        delete self;
    }

    return newcnt;
}

v3_result V3_API ct_plug_view::is_platform_type_supported(void *self_, const char *platform_type)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_gui_t *gui = comp->m_ext.m_gui;

    assert(gui);

    nonstd::string_view api_type = convert_clap_window_api_from_platform_type(platform_type);
    if (api_type.empty())
        return V3_FALSE;

    if (gui->is_api_supported(plug, api_type.data(), false))
        return V3_TRUE;
    if (gui->is_api_supported(plug, api_type.data(), true))
        return V3_TRUE;

    return V3_FALSE;
}

v3_result V3_API ct_plug_view::attached(void *self_, void *parent, const char *platform_type)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_gui_t *gui = comp->m_ext.m_gui;

    assert(gui);

    if (self->m_created)
        return V3_FALSE;

    bool window_valid = parent != nullptr;
    clap_window_t window{};
    if (!convert_to_clap_window(parent, platform_type, &window))
        return V3_FALSE;

    bool floating = !window_valid;
    if (!gui->create(plug, window.api, floating))
        return V3_FALSE;

    if (!floating)
        gui->set_parent(plug, &window);

    gui->show(plug);

    self->m_created = true;
    return V3_OK;
}

v3_result V3_API ct_plug_view::removed(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_gui_t *gui = comp->m_ext.m_gui;

    assert(gui);

    if (!self->m_created)
        return V3_FALSE;

    gui->destroy(plug);

    self->m_created = false;
    return V3_OK;
}

v3_result V3_API ct_plug_view::on_wheel(void *self_, float distance)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)distance;

    return V3_NOT_IMPLEMENTED;
}

v3_result V3_API ct_plug_view::on_key_down(void *self_, int16_t key_char, int16_t key_code, int16_t modifiers)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)key_char;
    (void)key_code;
    (void)modifiers;

    return V3_NOT_IMPLEMENTED;
}

v3_result V3_API ct_plug_view::on_key_up(void *self_, int16_t key_char, int16_t key_code, int16_t modifiers)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)key_char;
    (void)key_code;
    (void)modifiers;

    return V3_NOT_IMPLEMENTED;
}

v3_result V3_API ct_plug_view::get_size(void *self_, v3_view_rect *rect)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_gui_t *gui = comp->m_ext.m_gui;

    assert(gui);

    uint32_t width = 0;
    uint32_t height = 0;
    bool got_size = false;

    if (self->m_created) {
        got_size = gui->get_size(plug, &width, &height);
    }
    else {
        nonstd::string_view api = get_clap_default_window_api();
        // create temporary gui, only for size
        if (!gui->create(plug, api.data(), true)) {
            *rect = v3_view_rect{};
            return V3_FALSE;
        }
        got_size = gui->get_size(plug, &width, &height);
        gui->destroy(plug);
    }

    if (!got_size) {
        *rect = v3_view_rect{};
        return V3_FALSE;
    }

    rect->left = 0;
    rect->top = 0;
    rect->right = width;
    rect->bottom = height;
    return V3_OK;
}

v3_result V3_API ct_plug_view::on_size(void *self_, v3_view_rect *rect)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_gui_t *gui = comp->m_ext.m_gui;

    assert(gui);

    if (!self->m_created)
        return V3_FALSE;

    uint32_t width = (uint32_t)std::max(0, rect->right - rect->left);
    uint32_t height = (uint32_t)std::max(0, rect->bottom - rect->top);

    if (!gui->set_size(plug, width, height))
        return V3_FALSE;

    return V3_TRUE;
}

v3_result V3_API ct_plug_view::on_focus(void *self_, v3_bool state)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)state;

    return V3_NOT_IMPLEMENTED;
}

v3_result V3_API ct_plug_view::set_frame(void *self_, v3_plugin_frame **frame_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    v3::plugin_frame *frame = (v3::plugin_frame *)frame_;

    if (self->m_frame != frame) {
        v3::plugin_frame *old = self->m_frame;
        if (old)
            old->m_vptr->i_unk.unref(old);
        if (frame)
            frame->m_vptr->i_unk.ref(frame);
        self->m_frame = frame;
    }

    if (self->m_set_frame_hook)
        self->m_set_frame_hook(self);

    return V3_OK;
}

v3_result V3_API ct_plug_view::can_resize(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_gui_t *gui = comp->m_ext.m_gui;

    assert(gui);

    return gui->can_resize(plug);
}

v3_result V3_API ct_plug_view::check_size_constraint(void *self_, v3_view_rect *rect)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_plug_view *self = (ct_plug_view *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin_t *plug = comp->m_plug;
    const clap_plugin_gui_t *gui = comp->m_ext.m_gui;

    assert(gui);

    if (!self->m_created)
        return V3_FALSE;

    uint32_t width = (uint32_t)std::max(0, rect->right - rect->left);
    uint32_t height = (uint32_t)std::max(0, rect->bottom - rect->top);

    uint32_t adj_width = width;
    uint32_t adj_height = height;
    if (!gui->adjust_size(plug, &adj_width, &adj_height)) {
        adj_width = width;
        adj_height = height;
    }

    rect->right = rect->left + (int32_t)adj_width;
    rect->bottom = rect->top + (int32_t)adj_height;

    return V3_OK;
}
