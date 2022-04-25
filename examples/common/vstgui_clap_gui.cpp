#include "vstgui_clap_gui.hpp"
#include "vstgui_x11_runloop.hpp"
#include "include_vstgui.hpp"
VSTGUI_WARNINGS_PUSH
#if !defined(_WIN32) && !defined(__APPLE__)
#   include "vstgui/lib/platform/platform_x11.h"
#endif
#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
#include <vstgui/uidescription/editing/uieditcontroller.h>
#include <vstgui/lib/controls/ctextlabel.h>
#include <vstgui/lib/controls/cbuttons.h>
#endif
VSTGUI_WARNINGS_POP
#include <memory>
#include <cstdio>
#include <cstring>
using namespace VSTGUI;

//------------------------------------------------------------------------------
struct vstgui_clap_gui::internal {
    const clap_host_gui *clap_gui = nullptr;
    //
    bool frame_floating = false;
    bool frame_is_open = false;
    intptr_t frame_parent = 0;
    intptr_t frame_transient = 0;
#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    SharedPointer<UIDescription> uidesc;
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
    std::unique_ptr<X11::FrameConfig> frame_config;
#endif
#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    std::unique_ptr<IController> controller;
#endif
#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    SharedPointer<UIEditController> edit_controller;
    SharedPointer<CView> edit_view;
    std::unique_ptr<IControlListener> edit_bar_listener;
    bool is_editing = false;
#endif
    SharedPointer<CFrame> frame;
};

static void install_main_view(vstgui_clap_gui *gui, CView *view);

#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
static constexpr CCoord live_edit_bar_height = 25;
static void enable_editing(vstgui_clap_gui *gui, bool enable);
static CView *create_live_edit_bar(vstgui_clap_gui *gui);
#endif

//------------------------------------------------------------------------------
#if defined(_WIN32)
static const char *native_window_api = CLAP_WINDOW_API_WIN32;
static intptr_t get_native_window_id(const clap_window *w) { return (intptr_t)w->win32; }
#elif defined(__APPLE__)
static const char *native_window_api = CLAP_WINDOW_API_COCOA;
static intptr_t get_native_window_id(const clap_window *w) { return (intptr_t)w->cocoa; }
#else
static const char *native_window_api = CLAP_WINDOW_API_X11;
static intptr_t get_native_window_id(const clap_window *w) { return (intptr_t)w->x11; }
#endif

static vstgui_clap_gui *gui_from_plugin(const clap_plugin *plugin)
{
    return (vstgui_clap_gui *)plugin->get_extension(plugin, CLAP_EXT_GUI);
}

static bool vstgui_clap_gui__is_api_supported(const clap_plugin *plugin, const char *api, bool is_floating)
{
    (void)plugin;
    (void)is_floating;
    if (!std::strcmp(api, native_window_api))
        return true;
    return false;
}

static bool vstgui_clap_gui__get_preferred_api(const clap_plugin *plugin, const char **api, bool *is_floating)
{
    (void)plugin;
    *api = native_window_api;
    *is_floating = false;
    return true;
}

static bool vstgui_clap_gui__create(const clap_plugin *plugin, const char *api, bool is_floating)
{
    vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return false;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (priv)
        return false;

    if (std::strcmp(api, native_window_api))
        return false;

    priv = new vstgui_clap_gui::internal;
    gui->priv = priv;

    const clap_host *host = (const clap_host *)gui->reserved;
    priv->clap_gui = (const clap_host_gui *)host->get_extension(host, CLAP_EXT_GUI);

#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    UIDescription *uidesc = nullptr;
    if (gui->uidesc_file) {
        uidesc = new UIDescription{gui->uidesc_file};
        priv->uidesc = owned(uidesc);
        if (!uidesc->parse()) {
            delete priv;
            gui->priv = nullptr;
            return false;
        }
    }
#endif

    CRect size{};
#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    // get size from the UI description
    if (uidesc) {
        const std::string *size_str = nullptr;
        if (const UIAttributes *attr = uidesc->getViewAttributes("view"))
            size_str = attr->getAttributeValue("size");
        if (size_str) {
            unsigned w = 0, h = 0;
            if (std::sscanf(size_str->c_str(), "%u,%u", &w, &h) == 2) {
                size.setWidth((CCoord)w);
                size.setHeight((CCoord)h);
            }
        }
#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
        size.setHeight(size.getHeight() + live_edit_bar_height);
#endif
    }
#endif

    VSTGUIEditorInterface *ied = nullptr;
    CFrame *frame = new CFrame(size, ied);
    priv->frame = owned(frame);

    priv->frame_floating = is_floating;
    priv->frame_is_open = false;

    if (gui->on_init_frame) {
        if (!gui->on_init_frame(plugin)) {
            delete priv;
            gui->priv = nullptr;
            return false;
        }
    }

#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    // create the main view
    if (uidesc) {
        IController *controller = nullptr;
        if (gui->create_view_controller) {
            controller = gui->create_view_controller(plugin);
            priv->controller.reset(controller);
        }
        SharedPointer<CView> view = owned(uidesc->createView("view", controller));
        if (!view) {
            delete priv;
            gui->priv = nullptr;
            return false;
        }
        install_main_view(gui, view);
    }
#endif

    return true;
}

static void vstgui_clap_gui__destroy(const clap_plugin *plugin)
{
   vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return;

    if (CFrame *frame = priv->frame.get()) {
        if (gui->on_destroy_frame)
            gui->on_destroy_frame(plugin);
        priv->frame = nullptr;
    }

    if (vstgui_clap_gui::internal *priv = gui->priv) {
        delete priv;
        gui->priv = nullptr;
    }
}

static bool vstgui_clap_gui__set_scale(const clap_plugin *plugin, double scale)
{
    //TODO
    (void)plugin;

    if (scale != 1.0)
        return false;

    return true;
}

static bool vstgui_clap_gui__get_size(const clap_plugin *plugin, uint32_t *width, uint32_t *height)
{
    vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return false;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return false;

    CFrame *frame = priv->frame.get();
    if (!frame)
        return false;

    CRect rect = frame->getViewSize();
    *width = (uint32_t)rect.getWidth();
    *height = (uint32_t)rect.getHeight();
    return true;
}

static bool vstgui_clap_gui__can_resize(const clap_plugin *plugin)
{
    //TODO
    (void)plugin;
    return true;
}

static bool vstgui_clap_gui__get_resize_hints(const clap_plugin *plugin, clap_gui_resize_hints *hints)
{
    //TODO
    (void)plugin;
    (void)hints;
    return false;
}

static bool vstgui_clap_gui__adjust_size(const clap_plugin *plugin, uint32_t *width, uint32_t *height)
{
    //TODO
    (void)plugin;
    (void)width;
    (void)height;
    return true;
}

static bool vstgui_clap_gui__set_size(const clap_plugin *plugin, uint32_t width, uint32_t height)
{
    vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return false;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return false;

    CFrame *frame = priv->frame.get();
    if (!frame)
        return false;

    frame->setSize((CCoord)width, (CCoord)height);
    return true;
}

static bool vstgui_clap_gui__set_parent(const clap_plugin *plugin, const clap_window *window)
{
    vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return false;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return false;

    priv->frame_parent = get_native_window_id(window);
    return true;
}

static bool vstgui_clap_gui__set_transient(const clap_plugin *plugin, const clap_window *window)
{
    vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return false;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return false;

    priv->frame_transient = get_native_window_id(window);
    return true;
}

static void vstgui_clap_gui__suggest_title(const clap_plugin *plugin, const char *title)
{
    //XXX no vstgui support for window title
    (void)plugin;
    (void)title;
}

static bool vstgui_clap_gui__show(const clap_plugin *plugin)
{
    vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return false;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return false;

    CFrame *frame = priv->frame.get();
    if (!frame)
        return false;

    const clap_host *host = (const clap_host *)gui->reserved;

    if (!priv->frame_is_open) {
        void *system_window = nullptr;

        if (!priv->frame_floating)
            system_window = (void *)priv->frame_parent;
        //else
            //XXX: no vstgui support for transient

        IPlatformFrameConfig *frame_config = nullptr;
#if !defined(_WIN32) && !defined(__APPLE__)
        frame_config = priv->frame_config.get();
        if (!frame_config) {
            X11::FrameConfig *x11_config = new X11::FrameConfig;
            frame_config = x11_config;
            priv->frame_config.reset(x11_config);
            x11_config->runLoop = owned(new_vstgui_x11_runloop(host));
        }
#endif

        if (!frame->open(system_window, PlatformType::kDefaultNative, frame_config))
            return false;

        priv->frame_is_open = true;
    }

    frame->setVisible(true);
    return true;
}

static bool vstgui_clap_gui__hide(const clap_plugin *plugin)
{
    vstgui_clap_gui *gui = gui_from_plugin(plugin);
    if (!plugin)
        return false;

    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return false;

    CFrame *frame = priv->frame.get();
    if (!frame)
        return false;

    frame->setVisible(false);
    return true;
}

//------------------------------------------------------------------------------
void init_vstgui_clap_gui(vstgui_clap_gui *gui, const clap_host *host)
{
    gui->base.is_api_supported = &vstgui_clap_gui__is_api_supported;
    gui->base.get_preferred_api = &vstgui_clap_gui__get_preferred_api;
    gui->base.create = &vstgui_clap_gui__create;
    gui->base.destroy = &vstgui_clap_gui__destroy;
    gui->base.set_scale = &vstgui_clap_gui__set_scale;
    gui->base.get_size = &vstgui_clap_gui__get_size;
    gui->base.can_resize = &vstgui_clap_gui__can_resize;
    gui->base.get_resize_hints = &vstgui_clap_gui__get_resize_hints;
    gui->base.adjust_size = &vstgui_clap_gui__adjust_size;
    gui->base.set_size = &vstgui_clap_gui__set_size;
    gui->base.set_parent = &vstgui_clap_gui__set_parent;
    gui->base.set_transient = &vstgui_clap_gui__set_transient;
    gui->base.suggest_title = &vstgui_clap_gui__suggest_title;
    gui->base.show = &vstgui_clap_gui__show;
    gui->base.hide = &vstgui_clap_gui__hide;
    gui->reserved = (void *)host;
}

CFrame *get_vstgui_frame(vstgui_clap_gui *gui)
{
    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return nullptr;

    return priv->frame.get();
}

#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
UIDescription *get_vstgui_uidescription(vstgui_clap_gui *gui)
{
    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return nullptr;

    return priv->uidesc.get();
}
#endif

void update_vstgui_on_timer(vstgui_clap_gui *gui, clap_id timer_id)
{
    (void)gui;
    (void)timer_id;

#if !defined(_WIN32) && !defined(__APPLE__)
    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return;

    X11::FrameConfig *frame_config = priv->frame_config.get();
    if (!frame_config)
        return;

    X11::IRunLoop *runloop = frame_config->runLoop;
    if (!runloop)
        return;

    update_x11_runloop_on_timer(runloop, timer_id);
#endif
}

void update_vstgui_on_fd(vstgui_clap_gui *gui, int fd)
{
    (void)gui;
    (void)fd;

#if !defined(_WIN32) && !defined(__APPLE__)
    vstgui_clap_gui::internal *priv = gui->priv;
    if (!priv)
        return;

    X11::FrameConfig *frame_config = priv->frame_config.get();
    if (!frame_config)
        return;

    X11::IRunLoop *runloop = frame_config->runLoop;
    if (!runloop)
        return;

    update_x11_runloop_on_fd(runloop, fd);
#endif
}

//------------------------------------------------------------------------------
static void install_main_view(vstgui_clap_gui *gui, CView *view)
{
    vstgui_clap_gui::internal *priv = gui->priv;
    CFrame *frame = priv->frame.get();
    CRect rect = view->getViewSize();

    CRect frame_rect = rect;
#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    frame_rect.bottom += live_edit_bar_height;
#endif

    frame->removeAll();
    frame->setSize(frame_rect.getWidth(), frame_rect.getHeight());
    if (const clap_host_gui *clap_gui = priv->clap_gui) {
        const clap_host *host = (const clap_host *)gui->reserved;
        if (clap_gui->request_resize)
            clap_gui->request_resize(host, (uint32_t)frame_rect.getWidth(), (uint32_t)frame_rect.getHeight());
    }

    frame->addView(view);
    view->remember();

    CRect view_area = view->getViewSize();
    view_area.bottom = view_area.getHeight();
    view_area.top = 0;
#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    view_area.bottom += live_edit_bar_height;
    view_area.top += live_edit_bar_height;
#endif
    view->setViewSize(view_area);

#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    SharedPointer<CView> edit_bar = owned(create_live_edit_bar(gui));
    CRect edit_bar_rect{0, 0, view_area.getWidth(), live_edit_bar_height};
    edit_bar->setViewSize(edit_bar_rect);
    frame->addView(edit_bar);
    edit_bar->remember();
#endif
}

//------------------------------------------------------------------------------
#if defined(VSTGUI_LIVE_EDITING) && !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
class live_edit_control_listener : public IControlListener {
public:
    explicit live_edit_control_listener(vstgui_clap_gui *gui) : m_gui{gui} {}
    void valueChanged(CControl* control) override;

private:
    vstgui_clap_gui *m_gui = nullptr;
};

void live_edit_control_listener::valueChanged(CControl *control)
{
    int32_t tag = control->getTag();
    float value = control->getValue();

    if (tag == 'ui' && value) {
        enable_editing(m_gui, false);
    }
    else if (tag == 'edtr' && value) {
        enable_editing(m_gui, true);
    }
}

static void enable_editing(vstgui_clap_gui *gui, bool enable)
{
    vstgui_clap_gui::internal *priv = gui->priv;

    if (priv->is_editing == enable)
        return;

    if (enable) {
        SharedPointer<UIEditController> edit_controller = owned(new UIEditController{priv->uidesc});
        SharedPointer<CView> edit_view = owned(edit_controller->createEditView());
        if (!edit_view)
            return;
        edit_controller->remember();

        priv->edit_controller = edit_controller;
        priv->edit_view = edit_view;

        install_main_view(gui, edit_view);
    }
    else {
        UIDescription *uidesc = priv->uidesc;
        SharedPointer<CView> view = owned(uidesc->createView("view", priv->controller.get()));
        if (!view)
            return;

        install_main_view(gui, view);
    }

    priv->is_editing = enable;
}

static CView *create_live_edit_bar(vstgui_clap_gui *gui)
{
    vstgui_clap_gui::internal *priv = gui->priv;

    IControlListener *listener = priv->edit_bar_listener.get();
    if (!listener) {
        listener = new live_edit_control_listener{gui};
        priv->edit_bar_listener.reset(listener);
    }

    SharedPointer<CViewContainer> container = makeOwned<CViewContainer>(CRect{});
    container->setBackgroundColor(kGreyCColor);

    SharedPointer<CTextLabel> label = makeOwned<CTextLabel>(CRect{0, 0, 0, live_edit_bar_height}, "Live Editing", nullptr);
    SharedPointer<CFontDesc> label_font = makeOwned<CFontDesc>(*label->getFont());
    label->setBackColor(kTransparentCColor);
    label->setFrameColor(kTransparentCColor);
    label_font->setSize(12.0);
    label->setFont(label_font);
    label->sizeToFit();
    CRect label_area = label->getViewSize();
    label_area.left += 10;
    label_area.right += 10;
    label->setViewSize(label_area);
    container->addView(label);
    label->remember();

    CRect button1_area;
    button1_area.left = label_area.right + 40;
    button1_area.right = button1_area.left + 50;
    button1_area.top = 3;
    button1_area.bottom = live_edit_bar_height - 3;
    SharedPointer<CTextButton> button1 = makeOwned<CTextButton>(button1_area, listener, 'ui', "UI");
    container->addView(button1);
    button1->remember();

    CRect button2_area;
    button2_area.left = button1_area.right + 10;
    button2_area.right = button2_area.left + 50;
    button2_area.top = 3;
    button2_area.bottom = live_edit_bar_height - 3;
    SharedPointer<CTextButton> button2 = makeOwned<CTextButton>(button2_area, listener, 'edtr', "Editor");
    container->addView(button2);
    button2->remember();

    container->remember();
    return container;
}
#endif
