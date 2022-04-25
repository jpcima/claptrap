// Copyright (C) 2021-2022 Jean Pierre Cimalando <jp-dev@gmx.com>
// SPDX-License-Identifier: BSD-2-Clause

#pragma once
#include <atomic>

class spin_mutex {
public:
    void lock() noexcept;
    bool try_lock() noexcept;
    void unlock() noexcept;

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

inline bool spin_mutex::try_lock() noexcept
{
    return !flag_.test_and_set(std::memory_order_acquire);
}

inline void spin_mutex::unlock() noexcept
{
    flag_.clear(std::memory_order_release);
}
