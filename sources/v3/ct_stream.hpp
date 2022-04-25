#pragma once
#include "travesty_helpers.hpp"
#include <clap/clap.h>

struct ct_istream {
    explicit ct_istream(v3::bstream *stream);

    //--------------------------------------------------------------------------
    static int64_t read(const clap_istream *stream, void *buffer, uint64_t size);

    //--------------------------------------------------------------------------
    clap_istream m_istream {
        nullptr,
        &read,
    };
};

//------------------------------------------------------------------------------
struct ct_ostream {
    explicit ct_ostream(v3::bstream *stream);

    //--------------------------------------------------------------------------
    static int64_t write(const clap_ostream *stream, const void *buffer, uint64_t size);

    //--------------------------------------------------------------------------
    clap_ostream m_ostream {
        nullptr,
        &write,
    };
};
