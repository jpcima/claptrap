#if !defined(_WIN32) && !defined(__APPLE__)
#include "vstgui_x11_runloop.hpp"
#include "include_vstgui.hpp"
#include <clap/clap.h>
#include <memory>

class MyX11TimerSupport final {
public:
    explicit MyX11TimerSupport(const clap_host_t *host);
    bool registerTimer(uint64_t interval, VSTGUI::X11::ITimerHandler *handler);
    bool unregisterTimer(VSTGUI::X11::ITimerHandler *handler);

    //
    void dispatchTimerCallbacks(clap_id id);

private:
    const clap_host_t *m_host = nullptr;
    const clap_host_timer_support_t *m_support = nullptr;

    struct TimerSlot {
        clap_id id;
        VSTGUI::X11::ITimerHandler *handler = nullptr;
    };

    using TimerSlotPtr = std::unique_ptr<TimerSlot>;

    std::vector<TimerSlotPtr> m_slots;
};

MyX11TimerSupport::MyX11TimerSupport(const clap_host_t *host)
    : m_host{host}
{
    m_support = (const clap_host_timer_support_t *)host->get_extension(host, CLAP_EXT_TIMER_SUPPORT);
}

bool MyX11TimerSupport::registerTimer(uint64_t interval, VSTGUI::X11::ITimerHandler *handler)
{
    if (!handler)
        return false;

    const clap_host_t *host = m_host;
    const clap_host_timer_support_t *support = m_support;

    if (!support)
        return false;

    clap_id id = CLAP_INVALID_ID;
    if (!support->register_timer(host, (uint32_t)interval, &id))
        return false;

    size_t slot_idx = 0;
    size_t slot_count = m_slots.size();
    while (slot_idx < slot_count && m_slots[slot_idx])
        ++slot_idx;
    if (slot_idx == m_slots.size())
        m_slots.emplace_back();

    TimerSlot *slot = new TimerSlot;
    m_slots[slot_idx].reset(slot);

    slot->id = id;
    slot->handler = handler;

    return true;
}

bool MyX11TimerSupport::unregisterTimer(VSTGUI::X11::ITimerHandler *handler)
{
    if (!handler)
        return false;

    const clap_host_t *host = m_host;
    const clap_host_timer_support_t *support = m_support;

    if (!support)
        return false;

    bool found = false;
    size_t slot_idx = 0;
    size_t slot_count = m_slots.size();
    while (slot_idx < slot_count && !found) {
        TimerSlot *slot = m_slots[slot_idx].get();
        found = slot && slot->handler == handler;
        slot_idx += found ? 0 : 1;
    }

    if (!found)
        return false;

    if (!support->unregister_timer(host, m_slots[slot_idx]->id))
        return false;

    m_slots[slot_idx].reset();
    return true;
}

void MyX11TimerSupport::dispatchTimerCallbacks(clap_id id)
{
    bool done = false;
    size_t slot_idx = 0;
    size_t slot_count = m_slots.size();

    //TODO optimize O(n) search
    while (slot_idx < slot_count && !done) {
        TimerSlot *slot = m_slots[slot_idx].get();
        if (slot && slot->id == id) {
            slot->handler->onTimer();
            done = true;
        }
        ++slot_idx;
    }
}

//------------------------------------------------------------------------------
class MyX11FdSupport final
{
public:
    explicit MyX11FdSupport(const clap_host_t *host);
    bool registerEventHandler(int fd, VSTGUI::X11::IEventHandler *handler);
    bool unregisterEventHandler(VSTGUI::X11::IEventHandler *handler);

    //
    void dispatchFdCallbacks(int fd);

private:
    const clap_host_t *m_host = nullptr;
    const clap_host_posix_fd_support_t *m_support = nullptr;

    struct EventSlot {
        int fd;
        VSTGUI::X11::IEventHandler *handler = nullptr;
    };

    using EventSlotPtr = std::unique_ptr<EventSlot>;

    std::vector<EventSlotPtr> m_slots;
};

MyX11FdSupport::MyX11FdSupport(const clap_host_t *host)
    : m_host{host}
{
    m_support = (const clap_host_posix_fd_support_t *)host->get_extension(host, CLAP_EXT_POSIX_FD_SUPPORT);
}

bool MyX11FdSupport::registerEventHandler(int fd, VSTGUI::X11::IEventHandler *handler)
{
    if (fd == -1 || !handler)
        return false;

    const clap_host_t *host = m_host;
    const clap_host_posix_fd_support_t *support = m_support;

    if (!support)
        return false;

    if (!support->register_fd(host, fd, CLAP_POSIX_FD_READ))
        return false;

    size_t slot_idx = 0;
    size_t slot_count = m_slots.size();
    while (slot_idx < slot_count && m_slots[slot_idx])
        ++slot_idx;
    if (slot_idx == m_slots.size())
        m_slots.emplace_back();

    EventSlot *slot = new EventSlot;
    m_slots[slot_idx].reset(slot);

    slot->fd = fd;
    slot->handler = handler;

    return true;
}

bool MyX11FdSupport::unregisterEventHandler(VSTGUI::X11::IEventHandler *handler)
{
    if (!handler)
        return false;

    const clap_host_t *host = m_host;
    const clap_host_posix_fd_support_t *support = m_support;

    if (!support)
        return false;

    bool found = false;
    size_t slot_idx = 0;
    size_t slot_count = m_slots.size();
    while (slot_idx < slot_count && !found) {
        EventSlot *slot = m_slots[slot_idx].get();
        found = slot && slot->handler == handler;
        slot_idx += found ? 0 : 1;
    }

    if (!found)
        return false;

    if (!support->unregister_fd(host, m_slots[slot_idx]->fd))
        return false;

    m_slots[slot_idx].reset();
    return true;
}

void MyX11FdSupport::dispatchFdCallbacks(int fd)
{
    size_t slot_idx = 0;
    size_t slot_count = m_slots.size();

    //TODO optimize O(n) search
    while (slot_idx < slot_count) {
        EventSlot *slot = m_slots[slot_idx].get();
        if (slot && slot->fd == fd)
            slot->handler->onEvent();
        ++slot_idx;
    }
}

//------------------------------------------------------------------------------
class MyX11RunLoop final : public VSTGUI::X11::IRunLoop, public VSTGUI::AtomicReferenceCounted
{
public:
    explicit MyX11RunLoop(const clap_host_t *host);

    bool registerTimer(uint64_t interval, VSTGUI::X11::ITimerHandler *handler) override;
    bool unregisterTimer(VSTGUI::X11::ITimerHandler *handler) override;

    bool registerEventHandler(int fd, VSTGUI::X11::IEventHandler *handler) override;
    bool unregisterEventHandler(VSTGUI::X11::IEventHandler *handler) override;

    //
    void dispatchTimerCallbacks(clap_id id);
    void dispatchFdCallbacks(int fd);

private:
    MyX11TimerSupport m_timers;
    MyX11FdSupport m_fds;
};

MyX11RunLoop::MyX11RunLoop(const clap_host_t *host)
    : m_timers{host}, m_fds{host}
{
}

bool MyX11RunLoop::registerTimer(uint64_t interval, VSTGUI::X11::ITimerHandler *handler)
{
    return m_timers.registerTimer(interval, handler);
}

bool MyX11RunLoop::unregisterTimer(VSTGUI::X11::ITimerHandler *handler)
{
    return m_timers.unregisterTimer(handler);
}

bool MyX11RunLoop::registerEventHandler(int fd, VSTGUI::X11::IEventHandler *handler)
{
    return m_fds.registerEventHandler(fd, handler);
}

bool MyX11RunLoop::unregisterEventHandler(VSTGUI::X11::IEventHandler *handler)
{
    return m_fds.unregisterEventHandler(handler);
}

void MyX11RunLoop::dispatchTimerCallbacks(clap_id id)
{
    m_timers.dispatchTimerCallbacks(id);
}

void MyX11RunLoop::dispatchFdCallbacks(int fd)
{
    m_fds.dispatchFdCallbacks(fd);
}

//------------------------------------------------------------------------------
VSTGUI::X11::IRunLoop *new_vstgui_x11_runloop(const clap_host_t *host)
{
    return new MyX11RunLoop(host);
}

void update_x11_runloop_on_timer(VSTGUI::X11::IRunLoop *runloop_, clap_id timer_id)
{
    MyX11RunLoop *runloop = static_cast<MyX11RunLoop *>(runloop_);
    runloop->dispatchTimerCallbacks(timer_id);
}

void update_x11_runloop_on_fd(VSTGUI::X11::IRunLoop *runloop_, int fd)
{
    MyX11RunLoop *runloop = static_cast<MyX11RunLoop *>(runloop_);
    runloop->dispatchFdCallbacks(fd);
}

#endif // !defined(_WIN32) && !defined(__APPLE__)
