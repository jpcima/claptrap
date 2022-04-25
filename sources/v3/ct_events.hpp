#pragma once
#include <clap/clap.h>
#include <memory>
#include <cstdint>

class ct_events_buffer {
public:
    explicit ct_events_buffer(uint32_t capacity);
    bool add(const clap_event_header *event) noexcept;
    const clap_event_header *get(uint32_t index) const noexcept;
    void clear() noexcept;
    uint32_t count() const noexcept { return m_count; }

    void sort_events_by_time();

    clap_input_events as_clap_input() const noexcept;
    clap_output_events as_clap_output() noexcept;

private:
    std::unique_ptr<uint8_t[]> m_buf;
    uint32_t m_fill = 0;
    uint32_t m_capa = 0;
    std::unique_ptr<uint32_t[]> m_ind;
    uint32_t m_count = 0;
};
