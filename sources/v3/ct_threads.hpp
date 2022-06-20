#pragma once
#include "ct_defs.hpp"

// Provides some limited thread-safety on X11.
// This problem does not exist on Windows and MacOS platforms.
//
// In case the plugin runs without a UI loop, timers amd events are processed
// by a custom thread. This thread and the host thread run together with the
// [main-thread] role, potentially concurrently.
// The problem is partially mitigated by setting mutex guards at strategic
// positions. (eg. timer/event handler entries, component activation)
struct main_thread_guard {
#if CT_X11
    main_thread_guard();
    ~main_thread_guard();
#endif
};
