#pragma once

#if !defined(_WIN32) && !defined(__APPLE__)

#include <clap/clap.h>
namespace VSTGUI { namespace X11 { class IRunLoop; } }

VSTGUI::X11::IRunLoop *new_vstgui_x11_runloop(const clap_host_t *host);
void update_x11_runloop_on_timer(VSTGUI::X11::IRunLoop *runloop, clap_id timer_id);
void update_x11_runloop_on_fd(VSTGUI::X11::IRunLoop *runloop, int fd);

#endif // !defined(_WIN32) && !defined(__APPLE__)
