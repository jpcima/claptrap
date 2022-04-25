// Copyright (C) 2021-2022 Jean Pierre Cimalando <jp-dev@gmx.com>
// SPDX-License-Identifier: BSD-2-Clause

#include "spin_mutex.h"
#include "spin_loop_pause.h"
#include <thread>

// based on Timur Doumler's implementation advice for spinlocks

void spin_mutex::lock() noexcept
{
    for (int i = 0; i < 5; ++i) {
        if (try_lock())
            return;
    }

    for (int i = 0; i < 10; ++i) {
        if (try_lock())
            return;
        spin_loop_pause();
    }

    for (;;) {
        for (int i = 0; i < 3000; ++i) {
            if (try_lock())
                return;
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
            spin_loop_pause();
        }
        std::this_thread::yield();
    }
}
