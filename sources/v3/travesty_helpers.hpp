#pragma once
#include <travesty/base.h>
#include <travesty/host.h>
#include <travesty/bstream.h>
#include <travesty/edit_controller.h>
#include <travesty/audio_processor.h>
#include <travesty/events.h>
#include <travesty/view.h>

namespace v3 {

struct object {
    struct vtable {
        v3_funknown i_unk;
    };
    vtable *m_vptr;
};

struct host_application {
    struct vtable {
        v3_funknown i_unk;
        v3_host_application i_app;
    };
    vtable *m_vptr;
};

struct bstream {
    struct vtable {
        v3_funknown i_unk;
        v3_bstream i_stream;
    };
    vtable *m_vptr;
};

struct component_handler {
    struct vtable {
        v3_funknown i_unk;
        v3_component_handler i_hdr;
    };
    vtable *m_vptr;
};

struct component_handler2 {
    struct vtable {
        v3_funknown i_unk;
        v3_component_handler2 i_hdr;
    };
    vtable *m_vptr;
};

struct event_list {
    struct vtable {
        v3_funknown i_unk;
        v3_event_list i_list;
    };
    vtable *m_vptr;
};

struct param_value_queue {
    struct vtable {
        v3_funknown i_unk;
        v3_param_value_queue i_queue;
    };
    vtable *m_vptr;
};

struct param_changes {
    struct vtable {
        v3_funknown i_unk;
        v3_param_changes i_changes;
    };
    vtable *m_vptr;
};

struct plugin_frame {
    struct vtable {
        v3_funknown i_unk;
        v3_plugin_frame i_frame;
    };
    vtable *m_vptr;
};

struct run_loop {
    struct vtable {
        v3_funknown i_unk;
        v3_run_loop i_loop;
    };
    vtable *m_vptr;
};

} // namespace v3
