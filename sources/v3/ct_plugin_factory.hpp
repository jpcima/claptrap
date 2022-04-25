#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <travesty/factory.h>
#include <clap/clap.h>
#include <string>

struct ct_plugin_factory {
    explicit ct_plugin_factory(const clap_plugin_factory *cf);
    ~ct_plugin_factory();

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API get_factory_info(void *self, v3_factory_info *info);
    static int32_t V3_API num_classes(void *self);
    static v3_result V3_API get_class_info(void *self, int32_t idx, v3_class_info *info);
    static v3_result V3_API create_instance(void *self, const v3_tuid class_id, const v3_tuid iid, void **instance);

    //--------------------------------------------------------------------------
    static v3_result V3_API get_class_info_2(void *self, int32_t idx, v3_class_info_2 *info);

    //--------------------------------------------------------------------------
    static v3_result V3_API get_class_info_utf16(void *self, int32_t idx, v3_class_info_3 *info);
    static v3_result V3_API set_host_context(void *self, v3_funknown **host);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_plugin_factory i_v1 {
            &get_factory_info,
            &num_classes,
            &get_class_info,
            &create_instance,
        };
        const v3_plugin_factory_2 i_v2 {
            &get_class_info_2,
        };
        const v3_plugin_factory_3 i_v3 {
            &get_class_info_utf16,
            &set_host_context,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    const clap_plugin_factory *m_factory = nullptr;
    v3::object *m_hostcontext = nullptr;
};
