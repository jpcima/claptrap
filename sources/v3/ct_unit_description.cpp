#include "ct_unit_description.hpp"
#include "ct_component.hpp"
#include "utility/unicode_helpers.hpp"

namespace ct {

const ct_unit_description::vtable ct_unit_description::s_vtable;

ct_unit_description::ct_unit_description()
{
    // create the root unit
    unit_info *root = new unit_info;
    m_units.emplace_back(root);
    root->info.id = 0;
    root->info.parent_unit_id = -1;
    root->info.name[0] = 0;
    root->info.program_list_id = -1;
}

uint32_t ct_unit_description::get_or_create_module_unit(std::string_view module)
{
    // input: a '/'-separated module string such as "oscillators/wt1"

    uint32_t unit_id = 0;
    size_t idx = 0;
    size_t next;

    // go through the components one at a time
    while (idx < module.size()) {
        // isolate current component as [idx;next[
        next = idx;
        while (next < module.size() && module[next] != '/')
            ++next;

        // the full path so far
        std::string_view path{&module[0], next};
        // just the component
        std::string_view name{&module[idx], next - idx};

        // we may already have a unit by this path
        auto it = m_by_path.find(path);
        if (it != m_by_path.end()) {
            unit_id = it->second->info.id;
        }
        else {
            // make it as a child of the previous unit
            uint32_t parent_id = unit_id;
            unit_id = (uint32_t)m_units.size();

            // push it to the list
            unit_info *unit = new unit_info;
            m_units.emplace_back(unit);
            unit->info.id = unit_id;
            unit->info.parent_unit_id = parent_id;
            UTF_copy(unit->info.name, name);
            unit->info.program_list_id = -1;
            unit->path.assign(path);

            // make it known by path
            m_by_path[unit->path] = unit;
        }

        // advance 1 past '/'
        if (next < module.size())
            ++next;

        idx = next;
    }

    return unit_id;
}

v3_result V3_API ct_unit_description::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.query_interface(comp, iid, obj));
}

uint32_t V3_API ct_unit_description::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.ref(comp));
}

uint32_t V3_API ct_unit_description::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    ct_component *comp = self->m_comp;
    LOG_PLUGIN_RET(comp->m_vptr->i_unk.unref(comp));
}

int32_t V3_API ct_unit_description::get_unit_count(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    LOG_PLUGIN_RET(1);
}

v3_result V3_API ct_unit_description::get_unit_info(void *self_, int32_t unit_idx, v3_unit_info *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;

    if ((uint32_t)unit_idx >= self->m_units.size())
        LOG_PLUGIN_RET(V3_FALSE);

    *info = self->m_units[(uint32_t)unit_idx]->info;
    LOG_PLUGIN_RET(V3_OK);
}

int32_t V3_API ct_unit_description::get_program_list_count(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;

    LOG_PLUGIN_RET(0);
}

v3_result V3_API ct_unit_description::get_program_list_info(void *self_, int32_t list_idx, v3_program_list_info *info)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_idx;
    (void)info;

    LOG_PLUGIN_RET(V3_FALSE);
}

v3_result V3_API ct_unit_description::get_program_name(void *self_, int32_t list_id, int32_t program_idx, v3_str_128 name)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;
    (void)name;

    LOG_PLUGIN_RET(V3_FALSE);
}

v3_result V3_API ct_unit_description::get_program_info(void *self_, int32_t list_id, int32_t program_idx, const char *attribute_id, v3_str_128 attribute_value)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;
    (void)attribute_id;
    (void)attribute_value;

    LOG_PLUGIN_RET(V3_FALSE);
}

v3_result V3_API ct_unit_description::has_program_pitch_names(void *self_, int32_t list_id, int32_t program_idx)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;

    LOG_PLUGIN_RET(V3_FALSE);
}

v3_result V3_API ct_unit_description::get_program_pitch_name(void *self_, int32_t list_id, int32_t program_idx, int16_t midi_pitch, v3_str_128 name)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_id;
    (void)program_idx;
    (void)midi_pitch;
    (void)name;

    LOG_PLUGIN_RET(V3_FALSE);
}

int32_t V3_API ct_unit_description::get_selected_unit(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;
    LOG_PLUGIN_RET(self->m_selected_unit);
}

v3_result V3_API ct_unit_description::select_unit(void *self_, int32_t unit_id)
{
    LOG_PLUGIN_SELF_CALL(self_);

    ct_unit_description *self = (ct_unit_description *)self_;

    if ((uint32_t)unit_id >= self->m_units.size())
        LOG_PLUGIN_RET(V3_FALSE);

    self->m_selected_unit = (uint32_t)unit_id;
    LOG_PLUGIN_RET(V3_OK);
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

    LOG_PLUGIN_RET(V3_FALSE);
}

v3_result V3_API ct_unit_description::set_unit_program_data(void *self_, int32_t list_or_unit_id, int32_t program_idx, struct v3_bstream **data)
{
    LOG_PLUGIN_SELF_CALL(self_);

    (void)self_;
    (void)list_or_unit_id;
    (void)program_idx;
    (void)data;

    LOG_PLUGIN_RET(V3_FALSE);
}

} // namespace ct
