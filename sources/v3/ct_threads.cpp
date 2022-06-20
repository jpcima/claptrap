#include "ct_threads.hpp"
#include <mutex>

#if CT_X11
static std::recursive_mutex main_thread_guard_mutex;

main_thread_guard::main_thread_guard()
{
    main_thread_guard_mutex.lock();
}

main_thread_guard::~main_thread_guard()
{
    main_thread_guard_mutex.unlock();
}
#endif
