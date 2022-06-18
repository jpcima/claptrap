#include "ct_events.hpp"
#include "utility/ct_assert.hpp"
#include "libs/heapsort.hpp"
#include <cstring>

namespace ct {

ct_events_buffer::ct_events_buffer(uint32_t capacity)
{
    constexpr uint32_t alignval = alignof(clap_event_header);

    uint32_t aligned_capacity = (capacity + (alignval - 1)) & ~(alignval - 1);
    m_buf.reset(new uint8_t[aligned_capacity]);
    m_capa = aligned_capacity;

    uint32_t max_count = aligned_capacity / sizeof(clap_event_header);
    m_ind.reset(new uint32_t[max_count]);
}

bool ct_events_buffer::add(const clap_event_header *event) noexcept
{
    constexpr uint32_t alignval = alignof(clap_event_header);

    static_assert(sizeof(clap_event_header) > alignval);

    uint32_t size = event->size;
    CT_ASSERT(size >= sizeof(clap_event_header));

    uint32_t fill = m_fill;
    if (size > m_capa - fill)
        return false;

    CT_ASSERT(fill % alignval == 0);
    std::memcpy(&m_buf[fill], event, size);

    m_ind[m_count++] = fill;

    uint32_t aligned_size = (size + (alignval - 1)) & ~(alignval - 1);
    m_fill = fill + aligned_size;

    return true;
}

const clap_event_header *ct_events_buffer::get(uint32_t index) const noexcept
{
    if (index >= m_count)
        return nullptr;

    return (const clap_event_header *)&m_buf[m_ind[index]];
}

void ct_events_buffer::clear() noexcept
{
    m_fill = 0;
    m_count = 0;
}

void ct_events_buffer::sort_events_by_time()
{
    auto compare = [this](uint32_t a_pos, uint32_t b_pos) {
        const clap_event_header *a = (const clap_event_header *)&m_buf[a_pos];
        const clap_event_header *b = (const clap_event_header *)&m_buf[b_pos];
        return a->time < b->time;
    };
    heapsort::heapsort(&m_ind[0], &m_ind[m_count], compare);
}

clap_input_events ct_events_buffer::as_clap_input() const noexcept
{
    auto size = [](const clap_input_events *list) -> uint32_t {
        const ct_events_buffer *buf = (const ct_events_buffer *)list->ctx;
        return buf->count();
    };
    auto get = [](const clap_input_events *list, uint32_t index) -> const clap_event_header * {
        const ct_events_buffer *buf = (const ct_events_buffer *)list->ctx;
        return buf->get(index);
    };
    clap_input_events ie{const_cast<ct_events_buffer *>(this), +size, +get};
    return ie;
}

clap_output_events ct_events_buffer::as_clap_output() noexcept
{
    auto try_push = [](const clap_output_events *list, const clap_event_header *event) -> bool {
        ct_events_buffer *buf = (ct_events_buffer *)list->ctx;
        return buf->add(event);
    };
    clap_output_events oe{this, +try_push};
    return oe;
}

} // namespace ct
