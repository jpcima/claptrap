#include "ct_channel_mapping.hpp"
#include <clap/clap.h>
#include <algorithm>

static constexpr uint64_t v3_speaker_mono = (uint64_t)1 << 19;

bool ct_channel_remapper::configure(uint32_t nchan, nonstd::span<const uint8_t> channel_map)
{
    m_valid = false;
    m_v3_arrangement = 0;
    m_channel_count = 0;

    if (nchan > ct_port_max_channels)
        return false;

    uint32_t v3_arr;
    uint8_t *v3_to_clap = m_map_v3_to_clap;
    uint8_t *clap_to_v3 = m_map_clap_to_v3;

    // empty arrangement
    if (nchan == 0) {
        v3_arr = 0;
    }
    // mono arrangement
    else if (nchan == 1) {
        v3_arr = v3_speaker_mono;
        v3_to_clap[0] = 0;
        clap_to_v3[0] = 0;
    }
    // other arrangement with identity mapping
    else if (channel_map.empty()) {
        v3_arr = 0;
        for (uint32_t ch = 0; ch < nchan; ++ch) {
            uint64_t v3_spkr = (uint64_t)1 << ch;
            if (v3_spkr == v3_speaker_mono)
                return false; // cannot have Mono in a V3 arrangement

            v3_arr |= v3_spkr;
            v3_to_clap[ch] = ch;
            clap_to_v3[ch] = ch;
        }
    }
    // other arrangement with custom mapping
    else {
        if (channel_map.size() < nchan)
            return false; // channel map incomplete

        // first create a sorted version of the channel map,
        // by increasing channel ID
        struct map_entry {
            uint32_t clap_idx; // where channel appears in the unsorted map
            uint8_t channel; // channel ID
            bool operator<(const map_entry &oth) const noexcept
            {
                return channel < oth.channel;
            }
        };
        map_entry sorted_map[ct_port_max_channels];
        for (uint32_t i = 0; i < nchan; ++i) {
            sorted_map[i].clap_idx = i;
            sorted_map[i].channel = channel_map[i];
        }
        std::sort(sorted_map, sorted_map + nchan);

        // go through the sorted channels
        v3_arr = 0;
        for (uint32_t v3_idx = 0; v3_idx < nchan; ++v3_idx) {
            uint32_t clap_idx = sorted_map[v3_idx].clap_idx;
            uint8_t ch = sorted_map[v3_idx].channel;
            if (ch >= ct_port_max_channels)
                return false; // out of range

            uint64_t v3_spkr = (uint64_t)1 << ch;
            if (v3_spkr == v3_speaker_mono)
                return false; // cannot have Mono in a V3 arrangement
            if (v3_arr & v3_spkr)
                return false; // cannot have a duplicate speaker

            v3_arr |= v3_spkr;
            v3_to_clap[v3_idx] = clap_idx;
            clap_to_v3[clap_idx] = v3_idx;
        }
    }

    m_valid = true;
    m_v3_arrangement = v3_arr;
    m_channel_count = nchan;

    return true;
}

// NOTE: this implementation depends on V3 and CLAP having an identical channel
// numbering specification; however clap.surround is currently draft, so take a
// few precautions.
static_assert(CLAP_SURROUND_FL == 0);
static_assert(CLAP_SURROUND_FR == 1);
static_assert(CLAP_SURROUND_FC == 2);
static_assert(CLAP_SURROUND_LFE == 3);
static_assert(CLAP_SURROUND_BL == 4);
static_assert(CLAP_SURROUND_BR == 5);
static_assert(CLAP_SURROUND_FLC == 6);
static_assert(CLAP_SURROUND_FRC == 7);
static_assert(CLAP_SURROUND_BC == 8);
static_assert(CLAP_SURROUND_SL == 9);
static_assert(CLAP_SURROUND_SR == 10);
static_assert(CLAP_SURROUND_TC == 11);
static_assert(CLAP_SURROUND_TFL == 12);
static_assert(CLAP_SURROUND_TFC == 13);
static_assert(CLAP_SURROUND_TFR == 14);
static_assert(CLAP_SURROUND_TBL == 15);
static_assert(CLAP_SURROUND_TBC == 16);
static_assert(CLAP_SURROUND_TBR == 17);
