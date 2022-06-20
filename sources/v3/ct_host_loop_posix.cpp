#include "ct_host_loop_posix.hpp"

#if CT_X11
#include "travesty_helpers.hpp"
#include "utility/ct_assert.hpp"
#include <poll.h>
#include <limits.h>
#include <sys/timerfd.h>
#include <vector>
#include <set>
#include <map>
#include <optional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <limits>
#include <type_traits>
#include <cstdint>

namespace ct {

//------------------------------------------------------------------------------
struct threaded_run_loop::message {
    enum type {
        unknown,
        quit,
        register_fd,
        unregister_fd,
        register_timer,
        unregister_timer,
    };

    type m_type;
    int64_t m_data1;
    int64_t m_data2;

    struct completion {
        void notify(v3_result result);
        v3_result wait();
        std::optional<v3_result> m_result;
        std::mutex m_mutex;
        std::condition_variable m_cond;
    };

    completion *m_completion;
};

//------------------------------------------------------------------------------
class threaded_run_loop::background {
public:
    explicit background(threaded_run_loop *self);
    void join() { m_thread.join(); }

private:
    void run();
    v3_result process_message(const message &msg, bool *quit);

private:
    threaded_run_loop *m_self = nullptr;
    std::thread m_thread;

    //--------------------------------------------------------------------------
    bool m_pollfds_valid = false;
    std::vector<pollfd> m_pollfds;

    //--------------------------------------------------------------------------
    struct slot {
        enum type { fd, timer };
        type m_type;
        void *m_handler = nullptr;
        int m_fd = -1;
        // for timer
        uint64_t m_interval = 0;
    };

    std::map<void *, std::unique_ptr<slot>> m_slot_by_handler;
    std::map<int, std::set<slot *>> m_slot_by_fd;

    //--------------------------------------------------------------------------
    int create_timer_fd(uint64_t timeout);
    void release_timer_fd(uint64_t timeout);
    std::map<uint64_t, std::pair<posix_fd, size_t>> m_timer_fds;
};

//------------------------------------------------------------------------------
static threaded_run_loop *s_unique_instance = nullptr;

threaded_run_loop::threaded_run_loop()
{
    static_assert(std::is_pod_v<threaded_run_loop::message>);
    static_assert(sizeof(threaded_run_loop::message) <= PIPE_BUF);

    CT_ASSERT(s_unique_instance == nullptr);
    s_unique_instance = this;

    m_message_pipe = posix_pipe::create();
    m_background = std::make_unique<background>(this);
}

threaded_run_loop::~threaded_run_loop()
{
    message msg{};
    msg.m_type = message::quit;
    send_message(msg);
    m_background->join();

    CT_ASSERT(s_unique_instance == this);
    s_unique_instance = nullptr;
}

threaded_run_loop *threaded_run_loop::instance()
{
    threaded_run_loop *instance = s_unique_instance;
    if (!instance)
        instance = new threaded_run_loop;
    else
        instance->m_vptr->i_unk.ref(instance);
    return instance;
}

v3_result V3_API threaded_run_loop::query_interface(void *self_, const v3_tuid iid, void **obj)
{
    LOG_PLUGIN_SELF_CALL(self_);

    v3::object *result = nullptr;
    threaded_run_loop *self = (threaded_run_loop *)self_;

    if (!std::memcmp(iid, v3_funknown_iid, sizeof(v3_tuid)) ||
        !std::memcmp(iid, v3_run_loop_iid, sizeof(v3_tuid)))
    {
        result = (v3::object *)self;
    }

    if (result) {
        *obj = result;
        result->m_vptr->i_unk.ref(result);
        LOG_PLUGIN_RET(V3_OK);
    }
    else {
        *obj = nullptr;
        LOG_PLUGIN_RET(V3_NO_INTERFACE);
    }
}

uint32_t V3_API threaded_run_loop::ref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    threaded_run_loop *self = (threaded_run_loop *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_add(1, std::memory_order_relaxed);
    uint32_t newcnt = oldcnt + 1;

    LOG_PLUGIN_RET(newcnt);
}

uint32_t V3_API threaded_run_loop::unref(void *self_)
{
    LOG_PLUGIN_SELF_CALL(self_);

    threaded_run_loop *self = (threaded_run_loop *)self_;
    uint32_t oldcnt = self->m_refcnt.fetch_sub(1, std::memory_order_acq_rel);
    uint32_t newcnt = oldcnt - 1;

    if (newcnt == 0)
        delete self;

    LOG_PLUGIN_RET(newcnt);
}

v3_result V3_API threaded_run_loop::register_event_handler(void *self_, v3_event_handler **handler, int fd)
{
    LOG_PLUGIN_SELF_CALL(self_);

    threaded_run_loop *self = (threaded_run_loop *)self_;

    if (!handler || fd == -1)
        return V3_FALSE;

    message msg{};
    msg.m_type = message::register_fd;
    msg.m_data1 = (intptr_t)handler;
    msg.m_data2 = fd;
    message::completion completion;
    msg.m_completion = &completion;
    self->send_message(msg);
    return completion.wait();
}

v3_result V3_API threaded_run_loop::unregister_event_handler(void *self_, v3_event_handler **handler)
{
    LOG_PLUGIN_SELF_CALL(self_);

    threaded_run_loop *self = (threaded_run_loop *)self_;

    if (!handler)
        return V3_FALSE;

    message msg{};
    msg.m_type = message::unregister_fd;
    msg.m_data1 = (intptr_t)handler;
    message::completion completion;
    msg.m_completion = &completion;
    self->send_message(msg);
    return completion.wait();
}

v3_result V3_API threaded_run_loop::register_timer(void *self_, v3_timer_handler **handler, uint64_t ms)
{
    LOG_PLUGIN_SELF_CALL(self_);

    threaded_run_loop *self = (threaded_run_loop *)self_;

    if (!handler)
        return V3_FALSE;

    message msg{};
    msg.m_type = message::register_timer;
    msg.m_data1 = (intptr_t)handler;
    msg.m_data2 = (intptr_t)ms;
    message::completion completion;
    msg.m_completion = &completion;
    self->send_message(msg);
    return completion.wait();
}

v3_result V3_API threaded_run_loop::unregister_timer(void *self_, v3_timer_handler **handler)
{
    LOG_PLUGIN_SELF_CALL(self_);

    threaded_run_loop *self = (threaded_run_loop *)self_;

    if (!handler)
        return V3_FALSE;

    message msg{};
    msg.m_type = message::unregister_timer;
    msg.m_data1 = (intptr_t)handler;
    message::completion completion;
    msg.m_completion = &completion;
    self->send_message(msg);
    return completion.wait();
}

const threaded_run_loop::vtable threaded_run_loop::s_vtable;

//------------------------------------------------------------------------------
void threaded_run_loop::send_message(const message &msg)
{
    ssize_t count;
    int fd = m_message_pipe.writer_fd();
    do {
        count = write(fd, &msg, sizeof(msg));
    } while (count == -1 && errno == EINTR);
    CT_ASSERT(count == sizeof(msg));
}

void threaded_run_loop::receive_message(message &msg)
{
    ssize_t count;
    int fd = m_message_pipe.reader_fd();
    do {
        count = read(fd, &msg, sizeof(msg));
    } while (count == -1 && errno == EINTR);
    CT_ASSERT(count == sizeof(msg));
}

//------------------------------------------------------------------------------
threaded_run_loop::background::background(threaded_run_loop *self)
    : m_self{self}
{
    m_pollfds.reserve(64);
    m_thread = std::thread{[this]() { run(); }};
}

void threaded_run_loop::background::run()
{
    threaded_run_loop *self = m_self;
    bool quit = false;

    int message_fd = self->m_message_pipe.reader_fd();

    while (!quit) {
        std::vector<pollfd> &fds = m_pollfds;

        if (!m_pollfds_valid) {
            fds.clear();
            fds.push_back(pollfd{message_fd, POLLIN, 0});
            for (auto it = m_slot_by_fd.begin(); it != m_slot_by_fd.end(); ++it)
                fds.push_back(pollfd{it->first, POLLIN, 0});
            m_pollfds_valid = true;
        }

        int ret = poll(fds.data(), (nfds_t)fds.size(), -1);
        if (ret <= 0)
            continue;

        for (size_t i = 0; !quit && i < fds.size(); ++i) {
            if (fds[i].revents == 0)
                continue;

            int fd = fds[i].fd;
            if (fd == message_fd) {
                message msg{};
                self->receive_message(msg);
                v3_result result = process_message(msg, &quit);
                if (msg.m_completion)
                    msg.m_completion->notify(result);
            }
            else {
                auto it = m_slot_by_fd.find(fd);
                CT_ASSERT(it != m_slot_by_fd.end());
                for (slot *sl : it->second) {
                    switch (sl->m_type) {
                    case slot::fd:
                    {
                        v3::event_handler *handler = (v3::event_handler *)sl->m_handler;
                        handler->m_vptr->i_handler.on_fd_is_set(handler, fd);
                        break;
                    }
                    case slot::timer:
                    {
                        // read number from the descriptor
                        uint64_t expirations = 0;
                        read(fd, &expirations, sizeof(expirations));

                        v3::timer_handler *handler = (v3::timer_handler *)sl->m_handler;
                        handler->m_vptr->i_handler.on_timer(handler);
                        break;
                    }
                    default:
                        CT_ASSERT(false);
                    }
                }
            }

            fds[i].revents = 0;
        }
    }
}

v3_result threaded_run_loop::background::process_message(const message &msg, bool *quit)
{
    switch (msg.m_type) {
    case message::quit:
    {
        *quit = true;
        return V3_OK;
    }

    case message::register_fd:
    {
        v3::event_handler *handler = (v3::event_handler *)msg.m_data1;
        int fd = (int)msg.m_data2;

        if (m_slot_by_handler.find(handler) != m_slot_by_handler.end())
            return V3_FALSE;

        std::unique_ptr<slot> sl{new slot};
        sl->m_type = slot::fd;
        sl->m_handler = handler;
        sl->m_fd = fd;

        m_slot_by_fd[fd].insert(sl.get());
        m_slot_by_handler[handler] = std::move(sl);

        m_pollfds_valid = false;
        return V3_OK;
    }

    case message::unregister_fd:
    {
        v3::event_handler *handler = (v3::event_handler *)msg.m_data1;

        auto it = m_slot_by_handler.find(handler);
        if (it == m_slot_by_handler.end())
            return V3_FALSE;

        slot &sl = *it->second;
        if (sl.m_type != slot::fd)
            return V3_FALSE;

        m_slot_by_fd[sl.m_fd].erase(&sl);
        m_slot_by_handler.erase(handler);

        m_pollfds_valid = false;
        return V3_OK;
    }

    case message::register_timer:
    {
        v3::timer_handler *handler = (v3::timer_handler *)msg.m_data1;
        uint64_t ms = (uint64_t)msg.m_data2;

        if (m_slot_by_handler.find(handler) != m_slot_by_handler.end())
            return V3_FALSE;

        int fd = create_timer_fd((ms > 1) ? ms : 1);
        if (fd == -1)
            return V3_FALSE;

        std::unique_ptr<slot> sl{new slot};
        sl->m_type = slot::timer;
        sl->m_handler = handler;
        sl->m_fd = fd;
        sl->m_interval = ms;

        m_slot_by_fd[fd].insert(sl.get());
        m_slot_by_handler[handler] = std::move(sl);

        m_pollfds_valid = false;
        return V3_OK;
    }

    case message::unregister_timer:
    {
        v3::timer_handler *handler = (v3::timer_handler *)msg.m_data1;

        auto it = m_slot_by_handler.find(handler);
        if (it == m_slot_by_handler.end())
            return V3_FALSE;

        slot &sl = *it->second;
        if (sl.m_type != slot::timer)
            return V3_FALSE;

        release_timer_fd(sl.m_interval);

        m_slot_by_fd[sl.m_fd].erase(&sl);
        m_slot_by_handler.erase(handler);

        m_pollfds_valid = false;
        return V3_OK;
    }

    default:
        return V3_FALSE;
    }
}

//------------------------------------------------------------------------------
int threaded_run_loop::background::create_timer_fd(uint64_t timeout)
{
    auto it = m_timer_fds.find(timeout);

    if (it != m_timer_fds.end()) {
        it->second.second += 1;
        return it->second.first.get();
    }

    posix_fd tfd = posix_fd::owned(timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC));
    if (!tfd)
        return -1;

    if (timeout / 1000 > std::numeric_limits<time_t>::max())
        return -1;

    itimerspec spec;
    spec.it_interval.tv_sec = (time_t)(timeout / 1000);
    spec.it_interval.tv_nsec = (long)(1000000 * (timeout % 1000));
    spec.it_value = spec.it_interval;

    if (timerfd_settime(tfd.get(), 0, &spec, nullptr) == -1)
        return -1;

    int result = tfd.get();
    m_timer_fds[timeout] = std::make_pair(std::move(tfd), 1);
    return result;
}

void threaded_run_loop::background::release_timer_fd(uint64_t timeout)
{
    auto it = m_timer_fds.find(timeout);

    if (it == m_timer_fds.end())
        return;

    size_t count = it->second.second;
    if (count > 1)
        it->second.second = count - 1;
    else
        m_timer_fds.erase(it);
}

//------------------------------------------------------------------------------
void threaded_run_loop::message::completion::notify(v3_result result)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_result = result;
    m_cond.notify_one();
}

v3_result threaded_run_loop::message::completion::wait()
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_cond.wait(lock, [this]() -> bool { return m_result.has_value(); });
    v3_result result = m_result.value();
    m_result.reset();
    return result;
}

} // namespace ct

#endif
