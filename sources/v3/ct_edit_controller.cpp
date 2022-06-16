#include "ct_edit_controller.hpp"
#include "ct_component.hpp"
#include "ct_plug_view.hpp"
#include "ct_unit_description.hpp"
#include "ct_component_caches.hpp"
#include "unicode_helpers.hpp"
#include <cmath>
#include <cstring>

const ct_edit_controller::vtable ct_edit_controller::s_vtable;

v3_result V3_API ct_edit_controller::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.query_interface(comp, iid, obj));
}

uint32_t V3_API ct_edit_controller::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.ref(comp));
}

uint32_t V3_API ct_edit_controller::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.unref(comp));
}

v3_result V3_API ct_edit_controller::initialize(void *self_, v3_funknown** context)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_plug.initialize(comp, context));
}

v3_result V3_API ct_edit_controller::terminate(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_plug.terminate(comp));
}

v3_result V3_API ct_edit_controller::set_component_state(void *self_, v3_bstream **stream_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    // nothing to do
    (void)self_;
    (void)stream_;

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_edit_controller::set_state(void *self_, v3_bstream **stream_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;

    // make the controller update its cached parameter values
    // NOTE: the component should already have the state loaded
    // see VST3 documentation "Q: How does persistence work?"
    comp->sync_parameter_values_to_controller_from_plugin();

    // don't care about the state data
    (void)stream_;

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_edit_controller::get_state(void *self_, v3_bstream **stream_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    // nothing to do
    (void)self_;
    (void)stream_;

    LOG_PLUGIN_RET(V3_OK);
}

int32_t V3_API ct_edit_controller::get_parameter_count(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;

    LOG_PLUGIN_RET((uint32_t)comp->m_cache->get_params()->m_params.size());
}

v3_result V3_API ct_edit_controller::get_parameter_info(void *self_, int32_t param_idx, v3_param_info *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    ct_unit_description *units = comp->m_unit_description.get();
    const std::vector<clap_param_info> &params = comp->m_cache->get_params()->m_params;

    if ((uint32_t)param_idx >= params.size())
        LOG_PLUGIN_RET(V3_FALSE);

    const clap_param_info &ci = params[(uint32_t)param_idx];
    v3_param_id id = ci.id;

    uint32_t cf = ci.flags;
    info->flags = 0;
    info->flags |= (cf & CLAP_PARAM_IS_AUTOMATABLE) ? V3_PARAM_CAN_AUTOMATE : 0;
    info->flags |= (cf & CLAP_PARAM_IS_READONLY) ? V3_PARAM_READ_ONLY : 0;
    info->flags |= (cf & CLAP_PARAM_IS_PERIODIC) ? V3_PARAM_WRAP_AROUND : 0;
    //info->flags |= (cf & CLAP_PARAM_) ? V3_PARAM_IS_LIST : 0;
    info->flags |= (cf & CLAP_PARAM_IS_HIDDEN) ? V3_PARAM_IS_HIDDEN : 0;
    info->flags |= (cf & CLAP_PARAM_IS_BYPASS) ? V3_PARAM_IS_BYPASS : 0;

    info->param_id = id;
    UTF_copy(info->title, ci.name);
    info->short_title[0] = 0;
    info->units[0] = 0;
    if (cf & CLAP_PARAM_IS_STEPPED) {
        long min = (long)ci.min_value;
        long max = (long)ci.max_value;
        info->step_count = std::abs(max - min);
    }
    else {
        info->step_count = 0;
    }
    info->default_normalised_value = normalize_parameter_value(&ci, ci.default_value);
    info->unit_id = units->get_or_create_module_unit(ci.module);

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_edit_controller::get_parameter_string_for_value(void *self_, v3_param_id id, double normalised, v3_str_128 output_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_params *params = comp->m_ext.m_params;

    if (!params)
        LOG_PLUGIN_RET(V3_FALSE);

    double plain = normalised_parameter_to_plain(self_, id, normalised);
    char display[256] = {};
    if (!CLAP_CALL(params, value_to_text, plug, id, plain, display, sizeof(display)))
        LOG_PLUGIN_RET(V3_FALSE);

    nonstd::span<int16_t> output{output_, 128};
    UTF_copy(output, display);

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_edit_controller::get_parameter_value_for_string(void *self_, v3_param_id id, int16_t *input, double *output)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    const clap_plugin *plug = comp->m_plug;
    const clap_plugin_params *params = comp->m_ext.m_params;

    if (!params)
        LOG_PLUGIN_RET(V3_FALSE);

    char display[256] = {};
    UTF_copy(display, input);

    double plain = {};
    if (!CLAP_CALL(params, text_to_value, plug, id, display, &plain))
        LOG_PLUGIN_RET(V3_FALSE);

    double normalised = plain_parameter_to_normalised(self_, id, plain);
    *output = normalised;

    LOG_PLUGIN_RET(V3_OK);
}

double V3_API ct_edit_controller::normalised_parameter_to_plain(void *self_, v3_param_id id, double normalised)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    const clap_param_info *info = comp->m_cache->get_params()->get_param_by_id(id);

    if (!info)
        LOG_PLUGIN_RET(0);

    return denormalize_parameter_value(info, normalised);
}

double V3_API ct_edit_controller::plain_parameter_to_normalised(void *self_, v3_param_id id, double plain)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    const clap_param_info *info = comp->m_cache->get_params()->get_param_by_id(id);

    if (!info)
        LOG_PLUGIN_RET(0);

    return normalize_parameter_value(info, plain);
}

double V3_API ct_edit_controller::get_parameter_normalised(void *self_, v3_param_id id)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    const ct_caches::params_t *cache = comp->m_cache->get_params();

    const uint32_t *indexp = cache->m_param_idx_by_id.find(id);
    if (!indexp)
        LOG_PLUGIN_RET(0);

    uint32_t index = *indexp;
    CT_ASSERT(index < comp->m_param_value_cache.size());

    // obtain the plain value from cache
    double plain = comp->m_param_value_cache[index];

    double normalised = normalize_parameter_value(&cache->m_params[index], plain);
    LOG_PLUGIN_RET(normalised);
}

v3_result V3_API ct_edit_controller::set_parameter_normalised(void *self_, v3_param_id id, double normalised)
{
    LOG_PLUGIN_SELF_CALL(self_);

    //NOTE: using a cache; there isn't `plugin::set_value` in CLAP.
    // hosts might rely on a working sequence of
    // `set_parameter_normalised` + `get_parameter_normalised`

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    const ct_caches::params_t *cache = comp->m_cache->get_params();

    const uint32_t *indexp = cache->m_param_idx_by_id.find(id);
    if (!indexp)
        LOG_PLUGIN_RET(V3_FALSE);

    uint32_t index = *indexp;
    CT_ASSERT(index < comp->m_param_value_cache.size());

    // store the plain value into cache
    double plain = denormalize_parameter_value(&cache->m_params[index], normalised);
    comp->m_param_value_cache[index] = plain;

    LOG_PLUGIN_RET(V3_OK);
}

v3_result V3_API ct_edit_controller::set_component_handler(void *self_, v3_component_handler **handler_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    v3::component_handler *handler = (v3::component_handler *)handler_;

    if (comp->m_handler != handler) {
        v3::component_handler *old = comp->m_handler;
        if (old)
            old->m_vptr->i_unk.unref(old);
        if (handler)
            handler->m_vptr->i_unk.ref(handler);
        comp->m_handler = handler;

        v3::component_handler2 *handler2 = nullptr;
        comp->m_handler2 = (handler && handler->m_vptr->i_unk.query_interface(handler, v3_component_handler2_iid, (void **)&handler2) == V3_OK) ?
            handler2 : nullptr;
    }

    LOG_PLUGIN_RET(V3_OK);
}

v3_plugin_view **V3_API ct_edit_controller::create_view(void *self_, const char *name)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_edit_controller *self = (ct_edit_controller *)self_;
    ct_component *comp = self->m_comp;
    ct_plug_view *view = nullptr;

    if (!comp->m_ext.m_gui)
        LOG_PLUGIN_RET(nullptr);

    if (!std::strcmp(name, "editor")) {
        view = comp->m_editor;
        if (!view) {
            view = new ct_plug_view;
            view->m_comp = comp;
            view->m_delete_hook = +[](ct_plug_view *self)
            {
                ct_component *comp = self->m_comp;
                comp->m_editor = nullptr;
                comp->m_editor_frame = nullptr;
            };
            view->m_set_frame_hook = +[](ct_plug_view *self, v3::plugin_frame *frame)
            {
                ct_component *comp = self->m_comp;
                comp->m_editor_frame = frame;
#if CT_X11
                v3::run_loop *runloop = nullptr;
                if (frame) {
                    v3_result ret = frame->m_vptr->i_unk.query_interface(frame, v3_run_loop_iid, (void **)&runloop);
                    CT_ASSERT(ret == V3_OK);
                    (void)ret;
                }
                comp->set_run_loop(runloop);
#endif
            };
            comp->m_editor = view;
        }
        else {
            view->m_vptr->i_unk.ref(view);
        }
    }

    LOG_PLUGIN_RET_PTR((v3_plugin_view **)view);
}
