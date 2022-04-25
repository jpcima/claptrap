#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <travesty/unit.h>
#include <nonstd/string_view.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
struct ct_component;

// A subobject of `ct_component`
struct ct_unit_description {
    ct_unit_description();
    uint32_t get_or_create_module_unit(nonstd::string_view module);

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static int32_t V3_API get_unit_count(void *self);
    static v3_result V3_API get_unit_info(void *self, int32_t unit_idx, v3_unit_info *info);
    static int32_t V3_API get_program_list_count(void *self);
    static v3_result V3_API get_program_list_info(void *self, int32_t list_idx, v3_program_list_info *info);
    static v3_result V3_API get_program_name(void *self, int32_t list_id, int32_t program_idx, v3_str_128 name);
    static v3_result V3_API get_program_info(void *self, int32_t list_id, int32_t program_idx, const char *attribute_id, v3_str_128 attribute_value);
    static v3_result V3_API has_program_pitch_names(void *self, int32_t list_id, int32_t program_idx);
    static v3_result V3_API get_program_pitch_name(void *self, int32_t list_id, int32_t program_idx, int16_t midi_pitch, v3_str_128 name);
    static int32_t V3_API get_selected_unit(void *self);
    static v3_result V3_API select_unit(void *self, int32_t unit_id);
    static v3_result V3_API get_unit_by_bus(void *self, int32_t type, int32_t bus_direction, int32_t bus_idx, int32_t channel, int32_t *unit_id);
    static v3_result V3_API set_unit_program_data(void *self, int32_t list_or_unit_id, int32_t program_idx, struct v3_bstream **data);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_unit_information i_unit {
            &get_unit_count,
            &get_unit_info,
            &get_program_list_count,
            &get_program_list_info,
            &get_program_name,
            &get_program_info,
            &has_program_pitch_names,
            &get_program_pitch_name,
            &get_selected_unit,
            &select_unit,
            &get_unit_by_bus,
            &set_unit_program_data,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    ct_component *m_comp = nullptr;
    uint32_t m_selected_unit = 0;

    struct unit_info {
        v3_unit_info info; // unit ID must equal `m_units` index
        std::string path; // the module path from CLAP
    };

    std::vector<std::unique_ptr<unit_info>> m_units;
    std::unordered_map<nonstd::string_view, unit_info *> m_by_path;
};
