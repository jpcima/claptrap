#include "ct_stream.hpp"

ct_istream::ct_istream(v3::bstream *stream)
{
    m_istream.ctx = stream;
}

int64_t ct_istream::read(const clap_istream *stream, void *buffer, uint64_t size)
{
    v3::bstream *is = (v3::bstream *)stream->ctx;

    if (size > INT32_MAX)
        return -1;

    int32_t count = 0;
    if (is->m_vptr->i_stream.read(is, buffer, (int32_t)size, &count) != V3_OK)
        return -1;

    return count;
}

//------------------------------------------------------------------------------
ct_ostream::ct_ostream(v3::bstream *stream)
{
    m_ostream.ctx = stream;
}

int64_t ct_ostream::write(const clap_ostream *stream, const void *buffer, uint64_t size)
{
    v3::bstream *os = (v3::bstream *)stream->ctx;

    if (size > INT32_MAX)
        return -1;

    int32_t count = 0;
    if (os->m_vptr->i_stream.write(os, (void *)buffer, (int32_t)size, &count) != V3_OK)
        return -1;

    return count;
}
