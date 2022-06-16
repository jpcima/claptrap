#pragma once
#include "ct_defs.hpp"
#include "clap_id_map.hpp"
#include "clap_helpers.hpp"
#include <clap/clap.h>
#include <nonstd/span.hpp>
#include <vector>
#include <memory>
#include <cstdint>
struct ct_component;

struct ct_caches {
    explicit ct_caches(ct_component *comp);
    ~ct_caches();

    void invalidate_caches(uint32_t flags);
    void invalidate_all_caches() { invalidate_caches(~uint32_t{0}); }
    void update_caches_now();

    //--------------------------------------------------------------------------
    struct ports_t;
    struct ports_config_t;
    struct params_t;

    nonstd::span<const ports_config_t> get_audio_ports_configs();
    const ports_t *get_audio_ports();
    const params_t *get_params();

    void *m_callback_data = nullptr;
    void (*on_cache_update)(void *, uint32_t flags) = nullptr;

    //--------------------------------------------------------------------------
    enum cache_flag_t : uint32_t {
        cache_flags_audio_ports_config = 1 << 0,
        cache_flags_audio_ports = 1 << 1,
        cache_flags_params = 1 << 2,
    };

    //--------------------------------------------------------------------------
    struct ports_config_t {
        clap_audio_ports_config m_config;
        std::vector<ct_clap_port_info> m_inputs;
        std::vector<ct_clap_port_info> m_outputs;
        uint32_t m_total_channels = 0;
        bool m_can_do_64bit = false;
        const std::vector<ct_clap_port_info> &get_port_list(bool is_input) const;
    };

    struct ports_t {
        std::vector<ct_clap_port_info> m_inputs;
        std::vector<ct_clap_port_info> m_outputs;
        uint32_t m_total_channels = 0;
        bool m_can_do_64bit = false;
        const std::vector<ct_clap_port_info> &get_port_list(bool is_input) const;
    };

    struct params_t {
        std::vector<clap_param_info> m_params;
        clap_id_map<uint32_t, 1024> m_param_idx_by_id;
        const clap_param_info *get_param_by_id(clap_id id) const;
    };

    //--------------------------------------------------------------------------
    struct impl;
    std::unique_ptr<impl> m_priv;
};
