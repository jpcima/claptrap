#pragma once
#include "ct_defs.hpp"
#include "ct_channel_mapping.hpp"
#include <clap/clap.h>
#include <nonstd/span.hpp>
#include <nonstd/string_view.hpp>
#include <string>
#include <cstdint>

// convert plugin features to VST3 categories
std::string convert_categories_from_clap(const char *const *features);

// get the default stereo port setting, if plugin does not provide one
const clap_plugin_audio_ports *get_default_clap_plugin_audio_ports();

// convert CLAP window type to VST3
nonstd::string_view convert_clap_window_api_to_platform_type(nonstd::string_view api);
// convert VST3 window type to CLAP
nonstd::string_view convert_clap_window_api_from_platform_type(nonstd::string_view type);

// convert VST3 window to CLAP
bool convert_to_clap_window(void *wnd, nonstd::string_view platform_type, clap_window *window);

// get the CLAP window API for the platform
nonstd::string_view get_clap_default_window_api();

// extended port information, including the channel map
struct ct_clap_port_info : public clap_audio_port_info_t {
    ct_clap_port_info() : clap_audio_port_info_t{} {}
    ct_channel_remapper mapping;
};

// convert parameter to normalized
double normalize_parameter_value(const clap_param_info *info, double plain);
// convert parameter to plain
double denormalize_parameter_value(const clap_param_info *info, double normalized);
