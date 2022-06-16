#pragma once
#include "ct_defs.hpp"
#include "travesty_helpers.hpp"
#include <travesty/view.h>
#include <memory>
#include <atomic>
struct ct_component;
struct ct_plug_view_content_scale;

struct ct_plug_view {
    ct_plug_view();
    ~ct_plug_view();

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API is_platform_type_supported(void *self, const char *platform_type);
    static v3_result V3_API attached(void *self, void *parent, const char *platform_type);
    static v3_result V3_API removed(void *self);
    static v3_result V3_API on_wheel(void *self, float distance);
    static v3_result V3_API on_key_down(void *self, int16_t key_char, int16_t key_code, int16_t modifiers);
    static v3_result V3_API on_key_up(void *self, int16_t key_char, int16_t key_code, int16_t modifiers);
    static v3_result V3_API get_size(void *self, v3_view_rect *rect);
    static v3_result V3_API on_size(void *self, v3_view_rect *rect);
    static v3_result V3_API on_focus(void *self, v3_bool state);
    static v3_result V3_API set_frame(void *self, v3_plugin_frame **frame);
    static v3_result V3_API can_resize(void *self);
    static v3_result V3_API check_size_constraint(void *self, v3_view_rect *rect);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_plugin_view i_view {
            &is_platform_type_supported,
            &attached,
            &removed,
            &on_wheel,
            &on_key_down,
            &on_key_up,
            &get_size,
            &on_size,
            &on_focus,
            &set_frame,
            &can_resize,
            &check_size_constraint,
        };
    } s_vtable;

    //--------------------------------------------------------------------------
    const vtable *m_vptr = &s_vtable;
    std::atomic<unsigned> m_refcnt{1};
    ct_component *m_comp = nullptr;
    bool m_created = false;
    void (*m_delete_hook)(ct_plug_view *) = nullptr;
    void (*m_set_frame_hook)(ct_plug_view *, v3::plugin_frame *) = nullptr;
    v3::plugin_frame *m_frame = nullptr;

    // subobjects
    std::unique_ptr<ct_plug_view_content_scale> m_content_scale;
};
