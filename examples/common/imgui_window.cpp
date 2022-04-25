#include "imgui_window.hpp"
#include <imgui_impl_opengl2.h>
#include <atomic>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cassert>

// Logging function in printf-style
#define LOG_PRINTF(msg, ...) do {                                   \
        std::fprintf(stderr, "[ct-v3] " msg "\n", ## __VA_ARGS__);  \
        std::fflush(stderr);                                        \
    } while (0)

#define VERBOSE_PUGL_EVENTS 0

#if VERBOSE_PUGL_EVENTS
#   define LOG_PUGL_EVENT(e) log_pugl_event((e))
static void log_pugl_event(const PuglEvent *e);
#else
#   define LOG_PUGL_EVENT(e)
#endif

static std::atomic<size_t> s_instance_count;
static PuglWorld *s_world = nullptr;

ct_imgui_window::ct_imgui_window(const char *class_name, uint32_t default_width, uint32_t default_height, bool *init_ok)
    : m_class_name{class_name}
{
    s_instance_count.fetch_add(1, std::memory_order_relaxed);

    PuglWorld *world = s_world;
    if (!world) {
        world = puglNewWorld(PUGL_MODULE, PUGL_WORLD_THREADS);
        if (!world) {
            *init_ok = false;
            return;
        }
        s_world = world;
    }

    PuglView *view = puglNewView(world);
    if (!view) {
        *init_ok = false;
        return;
    }
    m_view = view;

    puglSetHandle(view, this);

    PuglRect initial_frame;
    initial_frame.x = 0;
    initial_frame.y = 0;
    initial_frame.width = default_width;
    initial_frame.height = default_height;

    if (puglSetBackend(view, puglGlBackend()) != PUGL_SUCCESS ||
        puglSetEventFunc(view, &on_event) != PUGL_SUCCESS ||
        puglSetDefaultSize(view, (int)default_width, (int)default_height) != PUGL_SUCCESS ||
        puglSetFrame(view, initial_frame) != PUGL_SUCCESS)
    {
        *init_ok = false;
        return;
    }

    #pragma message("TODO: view hints (resizable)")

    *init_ok = true;
}

ct_imgui_window::~ct_imgui_window()
{
    PuglWorld *world = s_world;
    PuglView *view = m_view;

    if (view)
        puglFreeView(view);

    if (s_instance_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        puglFreeWorld(world);
        s_world = nullptr;
    }
}

bool ct_imgui_window::set_parent(uintptr_t wnd)
{
    PuglView *view = m_view;

    if (!view)
        return false;

    return puglSetParentWindow(view, wnd) == PUGL_SUCCESS;
}

bool ct_imgui_window::set_transient(uintptr_t wnd)
{
    PuglView *view = m_view;

    if (!view)
        return false;

    return puglSetTransientParent(view, wnd) == PUGL_SUCCESS;
}

bool ct_imgui_window::set_caption(const char *text)
{
    PuglView *view = m_view;

    if (!view)
        return false;

    return puglSetWindowTitle(view, text) == PUGL_SUCCESS;
}

static const char *get_clipboard_text(void *self_)
{
    ct_imgui_window *self = (ct_imgui_window *)self_;
    PuglView *view = self->m_view;

    assert(view);

    self->m_inhibit_events = true;

    const char *type = nullptr;
    size_t len = 0;
    const char *data = (const char *)puglGetClipboard(view, &type, &len);

    self->m_inhibit_events = false;

    if (!data || !type || std::strcmp(type, "text/plain"))
        return nullptr;

    return data;
}

static void set_clipboard_text(void *self_, const char *text)
{
    ct_imgui_window *self = (ct_imgui_window *)self_;
    PuglView *view = self->m_view;

    assert(view);

    self->m_inhibit_events = true;

    puglSetClipboard(view, "text/plain", text, std::strlen(text));

    self->m_inhibit_events = false;
}

static void init_imgui(ct_imgui_window *self)
{
    if (self->m_imgui)
        return;

    IMGUI_CHECKVERSION();
    ImGuiContext *imgui = ImGui::CreateContext();
    self->m_imgui = imgui;
    ImGui::SetCurrentContext(imgui);

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    io.KeyMap[ImGuiKey_Tab] = '\t';
    io.KeyMap[ImGuiKey_LeftArrow] = 0xff + PUGL_KEY_LEFT - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_RightArrow] = 0xff + PUGL_KEY_RIGHT - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_UpArrow] = 0xff + PUGL_KEY_UP - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_DownArrow] = 0xff + PUGL_KEY_DOWN - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_PageUp] = 0xff + PUGL_KEY_PAGE_UP - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_PageDown] = 0xff + PUGL_KEY_PAGE_DOWN - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_Home] = 0xff + PUGL_KEY_HOME - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_End] = 0xff + PUGL_KEY_END - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_Insert] = 0xff + PUGL_KEY_INSERT - PUGL_KEY_F1;
    io.KeyMap[ImGuiKey_Delete] = PUGL_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = PUGL_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = ' ';
    io.KeyMap[ImGuiKey_Enter] = '\r';
    io.KeyMap[ImGuiKey_Escape] = PUGL_KEY_ESCAPE;
    //io.KeyMap[ImGuiKey_KeyPadEnter] = ;
    io.KeyMap[ImGuiKey_A] = 'a';
    io.KeyMap[ImGuiKey_C] = 'c';
    io.KeyMap[ImGuiKey_V] = 'v';
    io.KeyMap[ImGuiKey_X] = 'x';
    io.KeyMap[ImGuiKey_Y] = 'y';
    io.KeyMap[ImGuiKey_Z] = 'z';

    io.GetClipboardTextFn = &get_clipboard_text;
    io.SetClipboardTextFn = &set_clipboard_text;
    io.ClipboardUserData = self;

    ImGui_ImplOpenGL2_Init();
}

static void shutdown_imgui(ct_imgui_window *self)
{
    ImGuiContext *imgui = self->m_imgui;
    if (!imgui)
        return;

    ImGui::SetCurrentContext(imgui);

    ImGui_ImplOpenGL2_Shutdown();

    ImGui::DestroyContext(imgui);
    self->m_imgui = nullptr;
}

static void expose_imgui(ct_imgui_window *self)
{
    ImGuiContext *imgui = self->m_imgui;
    if (!imgui)
        return;

    ImGui::SetCurrentContext(imgui);

    PuglView *view = self->m_view;
    PuglRect rect = puglGetFrame(view);

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize.x = rect.width;
    io.DisplaySize.y = rect.height;

    ImGui_ImplOpenGL2_NewFrame();

    ImGui::NewFrame();
    if (self->m_imgui_callback)
        self->m_imgui_callback(self->m_imgui_callback_data);
    ImGui::Render();

    if (ImDrawData *data = ImGui::GetDrawData()) {
        glViewport(0, 0, rect.width, rect.height);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(data);
    }
}

static void handle_button_imgui(ct_imgui_window *self, const PuglEvent *event, bool is_press)
{
    ImGuiContext *imgui = self->m_imgui;
    if (!imgui)
        return;

    ImGui::SetCurrentContext(imgui);

    ImGuiIO &io = ImGui::GetIO();

    int button = event->button.button;
    if (button < 5)
        io.MouseDown[button] = is_press;
}

static void handle_motion_imgui(ct_imgui_window *self, const PuglEvent *event)
{
    ImGuiContext *imgui = self->m_imgui;
    if (!imgui)
        return;

    ImGui::SetCurrentContext(imgui);

    ImGuiIO &io = ImGui::GetIO();

    io.MousePos.x = event->motion.x;
    io.MousePos.y = event->motion.y;
}

static void handle_scroll_imgui(ct_imgui_window *self, const PuglEvent *event)
{
    ImGuiContext *imgui = self->m_imgui;
    if (!imgui)
        return;

    ImGui::SetCurrentContext(imgui);

    ImGuiIO &io = ImGui::GetIO();

    io.MouseWheel += event->scroll.dy;
    io.MouseWheelH += event->scroll.dx;
}

static void handle_key_imgui(ct_imgui_window *self, const PuglEvent *event, bool is_press)
{
    ImGuiContext *imgui = self->m_imgui;
    if (!imgui)
        return;

    ImGui::SetCurrentContext(imgui);

    ImGuiIO &io = ImGui::GetIO();

    io.KeyCtrl  = event->key.state & PUGL_MOD_CTRL;
    io.KeyShift = event->key.state & PUGL_MOD_SHIFT;
    io.KeyAlt   = event->key.state & PUGL_MOD_ALT;
    io.KeySuper = event->key.state & PUGL_MOD_SUPER;

    uint32_t key = event->key.key;

    if (key < 0x80)
        io.KeysDown[key] = is_press;
    else if (key >= PUGL_KEY_F1 && key <= PUGL_KEY_PAUSE)
        io.KeysDown[0xff + key - PUGL_KEY_F1] = is_press;
}

static void handle_text_imgui(ct_imgui_window *self, const PuglEvent *event)
{
    ImGuiContext *imgui = self->m_imgui;
    if (!imgui)
        return;

    ImGui::SetCurrentContext(imgui);

    ImGuiIO &io = ImGui::GetIO();

    uint32_t ch = event->text.character;
    if (ch < 0x20 || ch == 0x7f)
        return;

    io.AddInputCharactersUTF8(event->text.string);
}

PuglStatus ct_imgui_window::on_event(PuglView *view, const PuglEvent *event)
{
    ct_imgui_window *self = (ct_imgui_window *)puglGetHandle(view);

    LOG_PUGL_EVENT(event);

    if (self->m_inhibit_events) {
        return PUGL_SUCCESS;
    }

    if (event->type == PUGL_BUTTON_PRESS) {
        // this allows to get the key events
        puglGrabFocus(view);
    }

    switch (event->type) {
    case PUGL_CREATE:
        init_imgui(self);
        break;
    case PUGL_DESTROY:
        shutdown_imgui(self);
        break;
    case PUGL_EXPOSE:
        expose_imgui(self);
        break;

    case PUGL_BUTTON_PRESS:
        handle_button_imgui(self, event, true);
        break;
    case PUGL_BUTTON_RELEASE:
        handle_button_imgui(self, event, false);
        break;
    case PUGL_MOTION:
        handle_motion_imgui(self, event);
        break;
    case PUGL_SCROLL:
        handle_scroll_imgui(self, event);
        break;

    case PUGL_KEY_PRESS:
        handle_key_imgui(self, event, true);
        break;
    case PUGL_KEY_RELEASE:
        handle_key_imgui(self, event, false);
        break;
    case PUGL_TEXT:
        handle_text_imgui(self, event);
        break;

    default:
        break;
    }

    return PUGL_SUCCESS;
}

bool ct_imgui_window::show()
{
    PuglView *view = m_view;

    if (!view)
        return false;

    //NOTE: view gets realized here
    return puglShow(view) == PUGL_SUCCESS;
}

bool ct_imgui_window::hide()
{
    PuglView *view = m_view;

    if (!view)
        return false;

    return puglHide(view) == PUGL_SUCCESS;
}

bool ct_imgui_window::get_size(uint32_t *width, uint32_t *height)
{
    PuglView *view = m_view;

    if (!view)
        return false;

    PuglRect rect = puglGetFrame(view);
    *width = (uint32_t)rect.width;
    *height = (uint32_t)rect.height;
    return true;
}

bool ct_imgui_window::set_size(uint32_t width, uint32_t height)
{
    PuglView *view = m_view;

    if (!view)
        return false;

    PuglRect rect = puglGetFrame(view);
    rect.width = width;
    rect.height = height;
    return puglSetFrame(view, rect) == PUGL_SUCCESS;
}

bool ct_imgui_window::post_redisplay()
{
    PuglView *view = m_view;

    if (!view)
        return false;

    return puglPostRedisplay(view) == PUGL_SUCCESS;
}

bool ct_imgui_window::update(double timeout)
{
    PuglWorld *world = s_world;
    if (!world)
        return false;

    return puglUpdate(world, timeout) == PUGL_SUCCESS;
}

//------------------------------------------------------------------------------
#if VERBOSE_PUGL_EVENTS
#define EACH_PUGL_EVENT(F)                                              \
    F(0, NOTHING, void, "")                                                \
    F(1, CREATE, PuglCreateEvent, "")                                      \
    F(1, DESTROY, PuglDestroyEvent, "")                                    \
    F(1, CONFIGURE, PuglConfigureEvent, " pos=(%g;%g) size=(%g;%g)", x->x, x->y, x->width, x->height) \
    F(1, MAP, PuglMapEvent, "")                                            \
    F(1, UNMAP, PuglUnmapEvent, "")                                        \
    F(0, UPDATE, PuglUpdateEvent, "")                                      \
    F(0, EXPOSE, PuglExposeEvent, " pos=(%g;%g) size=(%g;%g)", x->x, x->y, x->width, x->height) \
    F(1, CLOSE, PuglCloseEvent, "")                                        \
    F(1, FOCUS_IN, PuglFocusEvent, "")                                     \
    F(1, FOCUS_OUT, PuglFocusEvent, "")                                    \
    F(1, KEY_PRESS, PuglKeyEvent, " code=%#x key=%#x", x->keycode, x->key) \
    F(1, KEY_RELEASE, PuglKeyEvent, " code=%#x key=%#x", x->keycode, x->key) \
    F(1, TEXT, PuglTextEvent, " text=\"%s\"", x->string)                   \
    F(1, POINTER_IN, PuglCrossingEvent, "")                                \
    F(1, POINTER_OUT, PuglCrossingEvent, "")                               \
    F(1, BUTTON_PRESS, PuglButtonEvent, " button=%u pos=(%g;%g)", x->button, x->x, x->y) \
    F(1, BUTTON_RELEASE, PuglButtonEvent, " button=%u pos=(%g;%g)", x->button, x->x, x->y) \
    F(1, MOTION, PuglMotionEvent, " pos=(%g;%g)", x->x, x->y)              \
    F(1, SCROLL, PuglScrollEvent, " delta=(%g;%g)", x->dx, x->dy)          \
    F(1, CLIENT, PuglClientEvent, "")                                      \
    F(1, TIMER, PuglTimerEvent, "")                                        \
    F(1, LOOP_ENTER, PuglLoopEnterEvent, "")                               \
    F(1, LOOP_LEAVE, PuglLoopLeaveEvent, "")                               \

static void log_pugl_event(const PuglEvent *e)
{
    switch (e->type) {
#define CASE_EVENT(logenable, value, type, fmt, ...)                    \
        case PUGL_##value: {                                            \
            if (!logenable) break;                                      \
            const type *x = (const type *)e; (void)x;                   \
            LOG_PRINTF("pugl event: %s" fmt, #value, ## __VA_ARGS__);   \
            break;                                                      \
        }

    EACH_PUGL_EVENT(CASE_EVENT)

    default:
        LOG_PRINTF("pugl event: unknown");

#undef CASE_EVENT
    }
}
#endif // VERBOSE_PUGL_EVENTS
