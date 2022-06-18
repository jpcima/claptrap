#pragma once
#include "ct_defs.hpp"
#include "libs/span.hpp"
#include <cstdint>

namespace ct {

class ct_channel_remapper {
public:
    bool configure(uint32_t nchan, nonstd::span<const uint8_t> channel_map);
    bool is_valid() const noexcept { return m_valid; }
    uint32_t get_v3_arrangement() const noexcept { return m_v3_arrangement; }

    enum : uint32_t { no_channel = ~(uint32_t)0 };

    uint32_t map_to_clap(uint32_t channel) const noexcept
    {
        if (channel >= m_channel_count)
            return no_channel;
        return m_map_v3_to_clap[channel];
    }

    uint32_t map_from_clap(uint32_t channel) const noexcept
    {
        if (channel >= m_channel_count)
            return no_channel;
        return m_map_clap_to_v3[channel];
    }

private:
    bool m_valid = false;
    uint32_t m_v3_arrangement = 0;
    uint32_t m_channel_count = 0;
    uint8_t m_map_v3_to_clap[ct_port_max_channels] = {};
    uint8_t m_map_clap_to_v3[ct_port_max_channels] = {};
};

} // namespace ct
