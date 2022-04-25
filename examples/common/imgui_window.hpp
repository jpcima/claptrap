#pragma once
#include <imgui.h>
#include <imgui_stdlib.h>
#include <pugl/pugl.h>
#include <pugl/gl.h>
#include <string>
#include <cstdint>

struct ct_imgui_window {
    ct_imgui_window(const char *class_name, uint32_t default_width, uint32_t default_height, bool *init_ok);
    ~ct_imgui_window();
    bool set_parent(uintptr_t wnd);
    bool set_transient(uintptr_t wnd);
    bool set_caption(const char *text);
    bool show();
    bool hide();
    bool get_size(uint32_t *width, uint32_t *height);
    bool set_size(uint32_t width, uint32_t height);
    bool post_redisplay();
    static bool update(double timeout);

    //--------------------------------------------------------------------------
    static PuglStatus on_event(PuglView *view, const PuglEvent *event);

    //--------------------------------------------------------------------------
    const std::string m_class_name;
    bool m_floating = false;
    PuglView *m_view = nullptr;
    ImGuiContext *m_imgui = nullptr;
    void *m_imgui_callback_data = nullptr;
    void (*m_imgui_callback)(void *) = nullptr;
    bool m_inhibit_events = false;
};
