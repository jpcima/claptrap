#pragma once
#include "ct_defs.hpp"

#if CT_X11
#include "utility/ct_posix_pipe.hpp"
#include <travesty/base.h>
#include <travesty/view.h>
#include <memory>
#include <atomic>

namespace ct {

struct threaded_run_loop {
private:
    threaded_run_loop();
    ~threaded_run_loop();

public:
    //--------------------------------------------------------------------------
    static threaded_run_loop *instance();

    //--------------------------------------------------------------------------
    static v3_result V3_API query_interface(void *self, const v3_tuid iid, void **obj);
    static uint32_t V3_API ref(void *self);
    static uint32_t V3_API unref(void *self);

    //--------------------------------------------------------------------------
    static v3_result V3_API register_event_handler(void *self, v3_event_handler **handler, int fd);
    static v3_result V3_API unregister_event_handler(void *self, v3_event_handler **handler);
    static v3_result V3_API register_timer(void *self, v3_timer_handler **handler, uint64_t ms);
    static v3_result V3_API unregister_timer(void *self, v3_timer_handler **handler);

    //--------------------------------------------------------------------------
    static const struct vtable {
        const v3_funknown i_unk {
            &query_interface,
            &ref,
            &unref,
        };
        const v3_run_loop i_loop {
            &register_event_handler,
            &unregister_event_handler,
            &register_timer,
            &unregister_timer,
        };
    } s_vtable;

private:
    //--------------------------------------------------------------------------
    std::atomic<unsigned> m_refcnt{1};
    ct::posix_pipe m_message_pipe;

    //--------------------------------------------------------------------------
    class background;
    std::unique_ptr<background> m_background;

    //--------------------------------------------------------------------------
    struct message;
    void send_message(const message &msg);
    void receive_message(message &msg);
};

} // namespace ct

#endif
