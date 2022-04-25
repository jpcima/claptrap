#include "ct_unit_description.hpp"
#include "ct_component.hpp"
#include "unicode_helpers.hpp"
#include <cstring>

const ct_unit_description::vtable ct_unit_description::s_vtable;

ct_unit_description::ct_unit_description()
{
    // create the root unit
    {
        m_units.emplace_back();
        unit_info &root = m_units.back();
        root.info.id = 0;
        root.info.parent_unit_id = -1;
        root.info.name[0] = 0;
        root.info.program_list_id = -1;
    }
}

v3_result V3_API ct_unit_description::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    ct_component *comp = self->m_comp;
    return comp->m_vptr->i_unk.query_interface(comp, iid, obj);
}

uint32_t V3_API ct_unit_description::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    ct_component *comp = self->m_comp;
    return comp->m_vptr->i_unk.ref(comp);
}

uint32_t V3_API ct_unit_description::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    ct_component *comp = self->m_comp;
    return comp->m_vptr->i_unk.unref(comp);
}

int32_t V3_API ct_unit_description::get_unit_count(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    return 1;
}

v3_result V3_API ct_unit_description::get_unit_info(void *self_, int32_t unit_idx, v3_unit_info *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;

    if ((uint32_t)unit_idx >= self->m_units.size())
        return V3_FALSE;

    std::memcpy(info, &self->m_units[(uint32_t)unit_idx].info, sizeof(v3_unit_info));
    return V3_OK;
}

int32_t V3_API ct_unit_description::get_program_list_count(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;

    return 0;
}

v3_result V3_API ct_unit_description::get_program_list_info(void *self_, int32_t list_idx, v3_program_list_info *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_idx;
    (void)info;

    return V3_FALSE;
}

v3_result V3_API ct_unit_description::get_program_name(void *self_, int32_t list_id, int32_t program_idx, v3_str_128 name)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;
    (void)name;

    return V3_FALSE;
}

v3_result V3_API ct_unit_description::get_program_info(void *self_, int32_t list_id, int32_t program_idx, const char *attribute_id, v3_str_128 attribute_value)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;
    (void)attribute_id;
    (void)attribute_value;

    return V3_FALSE;
}

v3_result V3_API ct_unit_description::has_program_pitch_names(void *self_, int32_t list_id, int32_t program_idx)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;

    return V3_FALSE;
}

v3_result V3_API ct_unit_description::get_program_pitch_name(void *self_, int32_t list_id, int32_t program_idx, int16_t midi_pitch, v3_str_128 name)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;
    (void)midi_pitch;
    (void)name;

    return V3_FALSE;
}

int32_t V3_API ct_unit_description::get_selected_unit(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    return self->m_selected_unit;
}

v3_result V3_API ct_unit_description::select_unit(void *self_, int32_t unit_id)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;

    if ((uint32_t)unit_id >= self->m_units.size())
        return V3_FALSE;

    self->m_selected_unit = (uint32_t)unit_id;
    return V3_OK;
}

v3_result V3_API ct_unit_description::get_unit_by_bus(void *self_, int32_t type, int32_t bus_direction, int32_t bus_idx, int32_t channel, int32_t *unit_id)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)type;
    (void)bus_direction;
    (void)bus_idx;
    (void)channel;
    (void)unit_id;

    return V3_FALSE;
}

v3_result V3_API ct_unit_description::set_unit_program_data(void *self_, int32_t list_or_unit_id, int32_t program_idx, struct v3_bstream **data)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_or_unit_id;
    (void)program_idx;
    (void)data;

    return V3_FALSE;
}
