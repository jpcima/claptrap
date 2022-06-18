#pragma once
#include "include_vstgui.hpp"
VSTGUI_WARNINGS_PUSH
#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
#include <vstgui/uidescription/uidescription.h>
#include <vstgui/uidescription/uiattributes.h>
#include <vstgui/uidescription/icontroller.h>
#endif
VSTGUI_WARNINGS_POP
#include <clap/clap.h>

struct vstgui_clap_gui {
    struct internal;

    // set by init, do not modify these
    clap_plugin_gui base = {};

#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    // optional name of the UI description file, used when creating
    const char *uidesc_file = nullptr;
#endif

    // --- user callbacks --- //

    // called when frame has been created
    bool (*on_init_frame)(const clap_plugin_t *plugin) = nullptr;

    // called when frame is about to be destroyed
    void (*on_destroy_frame)(const clap_plugin_t *plugin) = nullptr;

#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
    // creates the controller which will be associated with the view
    VSTGUI::IController *(*create_view_controller)(const clap_plugin_t *plugin) = nullptr;
#endif

    // --- internal --- //

    // do not modify
    internal *priv = nullptr;
    void *reserved = nullptr;
};

// initialization; call this in `clap_plugin_t::init`
void init_vstgui_clap_gui(vstgui_clap_gui *gui, const clap_host *host);

// get the frame; this is valid after a successful `clap_plugin_gui_t::create`
VSTGUI::CFrame *get_vstgui_frame(vstgui_clap_gui *gui);

#if !defined(DISABLE_VSTGUI_CLAP_UIDESCRIPTION)
// get the uidescription; this is valid after a successful `clap_plugin_gui_t::create`
VSTGUI::UIDescription *get_vstgui_uidescription(vstgui_clap_gui *gui);
#endif

// timer update; call this in `clap_plugin_timer_support_t::on_timer`
void update_vstgui_on_timer(vstgui_clap_gui *gui, clap_id timer_id);

// fd update; call this in `clap_plugin_posix_fd_support_t::on_fd`
void update_vstgui_on_fd(vstgui_clap_gui *gui, int fd);
