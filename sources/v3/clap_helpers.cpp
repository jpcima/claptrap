#include "clap_helpers.hpp"
#include "ct_defs.hpp"
#include <list>
#include <set>
#include <unordered_map>
#include <array>
#include <cstring>

namespace ct {

std::string convert_categories_from_clap(const char *const *features)
{
    std::set<std::string_view> cats;

    static const std::unordered_map<std::string_view, std::list<std::string_view>> clap_to_vst3
    {
        {"analyzer", {"Analyzer"}},
        {"audio_effect", {"Fx"}},
        {"delay", {"Delay"}},
        {"distortion", {"Distortion"}},
        {"drum", {"Drum"}},
        {"dynamics", {"Dynamics"}},
        {"equalizer", {"EQ"}},
        {"external", {"External"}},
        {"filter", {"Filter"}},
        {"generator", {"Generator"}},
        {"instrument", {"Instrument"}},
        {"mastering", {"Mastering"}},
        {"modulation", {"Modulation"}},
        {"mono", {"Mono"}},
        {"network", {"Network"}},
        {"note_effect", {"Fx"}},
        {"piano", {"Piano"}},
        {"pitch_shift", {"Pitch Shift"}},
        {"restoration", {"Restoration"}},
        {"reverb", {"Reverb"}},
        {"sampler", {"Sampler"}},
        {"spatial", {"Spatial"}},
        {"stereo", {"Stereo"}},
        {"surround", {"Surround"}},
        {"synth", {"Synth"}},
        {"tool", {"Tools"}},
    };

    for (const char *const *featp = features; *featp; ++featp) {
        auto it = clap_to_vst3.find(*featp);
        if (it != clap_to_vst3.end()) {
            for (std::string_view cat : it->second)
                cats.insert(cat);
        }
    }

    std::string text;
    if (!cats.empty()) {
        size_t size = cats.size() - 1;
        for (std::string_view cat : cats)
            size += cat.size();
        text.reserve(size);
        for (std::string_view cat : cats) {
            if (!text.empty()) text.push_back('|');
            text.append(cat.data(), cat.size());
        }
    }
    return text;
}

const clap_plugin_audio_ports *get_default_clap_plugin_audio_ports()
{
   static auto count = [](const clap_plugin *plugin, bool is_input) -> uint32_t
   {
       (void)plugin;
       (void)is_input;
       return 1;
   };

   static auto get = [](const clap_plugin *plugin, uint32_t index, bool is_input, clap_audio_port_info *info) -> bool
   {
       (void)plugin;
       if (index != 0)
           return false;
       if (is_input) {
           info->id = 0;
           std::strcpy(info->name, "Input");
       }
       else {
           info->id = 1;
           std::strcpy(info->name, "Output");
       }
       info->flags = 0;
       info->channel_count = 2;
       info->port_type = CLAP_PORT_STEREO;
       info->in_place_pair = CLAP_INVALID_ID;
       return true;
   };

    static const clap_plugin_audio_ports ext = {
        +count,
        +get,
    };

    return &ext;
}

std::string_view convert_clap_window_api_to_platform_type(std::string_view api)
{
#if defined(_WIN32)
    if (api == CLAP_WINDOW_API_WIN32)
        return "HWND";
#elif defined(__APPLE__)
    if (api == CLAP_WINDOW_API_COCOA)
        return "NSView";
#elif CT_X11
    if (api == CLAP_WINDOW_API_X11)
        return "X11EmbedWindowID";
#endif
    return "";
}

std::string_view convert_clap_window_api_from_platform_type(std::string_view type)
{
#if defined(_WIN32)
    if (type == "HWND")
        return CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
    if (type == "NSView")
        return CLAP_WINDOW_API_COCOA;
#elif CT_X11
    if (type == "X11EmbedWindowID")
        return CLAP_WINDOW_API_X11;
#endif
    return "";
}

bool convert_to_clap_window(void *wnd, std::string_view platform_type, clap_window *window)
{
#if defined(_WIN32)
    if (platform_type == "HWND") {
        window->api = CLAP_WINDOW_API_WIN32;
        window->win32 = (clap_hwnd)wnd;
        return true;
    }
#elif defined(__APPLE__)
    if (platform_type == "NSView") {
        window->api = CLAP_WINDOW_API_COCOA;
        window->cocoa = (clap_nsview)wnd;
        return true;
    }
#elif CT_X11
    if (platform_type == "X11EmbedWindowID") {
        window->api = CLAP_WINDOW_API_X11;
        window->x11 = (clap_xwnd)wnd;
        return true;
    }
#endif
    return false;
}

std::string_view get_clap_default_window_api()
{
#if defined(_WIN32)
    return CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
    return CLAP_WINDOW_API_COCOA;
#elif CT_X11
    return CLAP_WINDOW_API_X11;
#endif
}

double normalize_parameter_value(const clap_param_info *info, double plain)
{
    double min = info->min_value;
    double max = info->max_value;
    if (info->flags & CLAP_PARAM_IS_STEPPED) {
        min = (double)(long)min;
        max = (double)(long)max;
    }

    if (max == min)
        return min;

    return (plain - min) / (max - min);
}

double denormalize_parameter_value(const clap_param_info *info, double normalized)
{
    double ret;

    if (info->flags & CLAP_PARAM_IS_STEPPED) {
        long min = (long)info->min_value;
        long max = (long)info->max_value;
        ret = min + (long)std::min((double)(max - min), normalized * (max - min + 1));
    }
    else {
        double min = info->min_value;
        double max = info->max_value;
        ret = min + normalized * (max - min);
    }

    return ret;
}

} // namespace ct
