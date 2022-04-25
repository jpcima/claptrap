#include "../common/imgui_window.hpp"
#include <clap/clap.h>
#include <memory>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cassert>

enum {
    GAIN__AUDIO_INPUT_MAIN = 0,
    GAIN__NUM_INPUTS = 1,
};

enum {
    GAIN__AUDIO_OUTPUT_MAIN = 0,
    GAIN__NUM_OUTPUTS = 1,
};

enum {
    GAIN__AUDIO_PORT_CONFIG_MONO = 0,
    GAIN__AUDIO_PORT_CONFIG_STEREO = 1,
    GAIN__AUDIO_PORT_CONFIG__COUNT = 2,
};

enum {
    GAIN__PARAM_GAIN = 0,
    GAIN__PARAM__COUNT = 1,
};

//------------------------------------------------------------------------------
struct gain__param_values_t {
    double gain = 1;
};

struct gain__private_t {
    const clap_host_t *host = nullptr;

    //--------------------------------------------------------------------------
    struct {
        const clap_host_timer_support_t *timer_support;
    } host_ext;

    //--------------------------------------------------------------------------
    clap_id audio_port_config_id = 0;
    gain__param_values_t param = {};

    //--------------------------------------------------------------------------
    std::unique_ptr<ct_imgui_window> window;
    clap_id gui_timer_id = CLAP_INVALID_ID;
};

static bool gain__init(const clap_plugin_t *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    const clap_host_t *host = priv->host;
    priv->host_ext.timer_support = (const clap_host_timer_support_t *)host->get_extension(host, CLAP_EXT_TIMER_SUPPORT);
    return true;
}

static void gain__destroy(const clap_plugin_t *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    delete priv;
    delete plugin;
}

static bool gain__activate(
    const clap_plugin_t *plugin, double sample_rate,
    uint32_t min_frames_count, uint32_t max_frames_count)
{
    (void)plugin;
    (void)sample_rate;
    (void)min_frames_count;
    (void)max_frames_count;
    return true;
}

static void gain__deactivate(const clap_plugin_t *plugin)
{
    (void)plugin;
}

static bool gain__start_processing(const clap_plugin_t *plugin)
{
    (void)plugin;
    return true;
}

static void gain__stop_processing(const clap_plugin_t *plugin)
{
    (void)plugin;
}

static void gain__reset(const clap_plugin_t *plugin)
{
    (void)plugin;
}

static clap_process_status gain__process(
    const clap_plugin_t *plugin, const clap_process_t *process)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    uint32_t nf = process->frames_count;
    if (nf < 1)
        return CLAP_PROCESS_CONTINUE;

    clap_audio_buffer_t in = process->audio_inputs[0];
    clap_audio_buffer_t out = process->audio_outputs[0];
    uint32_t nc = in.channel_count;
    assert(nc == out.channel_count);

    float k = (float)priv->param.gain;
    for (uint32_t c = 0; c < nc; ++c) {
        for (uint32_t i = 0; i < nf; ++i)
            out.data32[c][i] = k * in.data32[c][i];
    }

    return CLAP_PROCESS_CONTINUE;
}

//------------------------------------------------------------------------------
static uint32_t gain__audio_ports__count(const clap_plugin_t *plugin, bool is_input)
{
    (void)plugin;
    (void)is_input;
    return is_input ? GAIN__NUM_INPUTS : GAIN__NUM_OUTPUTS;
}

static bool gain__audio_ports__get(
    const clap_plugin_t *plugin, uint32_t index, bool is_input,
    clap_audio_port_info_t *info)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (is_input) {
        switch (index) {
        case 0:
            info->id = GAIN__AUDIO_INPUT_MAIN;
            std::strcpy(info->name, "Input");
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            if (priv->audio_port_config_id == GAIN__AUDIO_PORT_CONFIG_MONO) {
                info->channel_count = 1;
                info->port_type = CLAP_PORT_MONO;
            }
            else if (priv->audio_port_config_id == GAIN__AUDIO_PORT_CONFIG_STEREO) {
                info->channel_count = 2;
                info->port_type = CLAP_PORT_STEREO;
            }
            else {
                return false;
            }
            info->in_place_pair = GAIN__AUDIO_OUTPUT_MAIN;
            return true;
        default:
            return false;
        }
    }
    else {
        switch (index) {
        case 0:
            info->id = GAIN__AUDIO_OUTPUT_MAIN;
            std::strcpy(info->name, "Output");
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            if (priv->audio_port_config_id == GAIN__AUDIO_PORT_CONFIG_MONO) {
                info->channel_count = 1;
                info->port_type = CLAP_PORT_MONO;
            }
            else if (priv->audio_port_config_id == GAIN__AUDIO_PORT_CONFIG_STEREO) {
                info->channel_count = 2;
                info->port_type = CLAP_PORT_STEREO;
            }
            else {
                return false;
            }
            info->in_place_pair = GAIN__AUDIO_INPUT_MAIN;
            return true;
        default:
            return false;
        }
    }
}

uint32_t gain__audio_ports_config__count(const clap_plugin_t *plugin)
{
    (void)plugin;
    return GAIN__AUDIO_PORT_CONFIG__COUNT;
}

bool gain__audio_ports_config__get(const clap_plugin_t *plugin, uint32_t index, clap_audio_ports_config_t *config)
{
    (void)plugin;

    config->input_port_count = 1;
    config->output_port_count = 1;
    config->has_main_input = true;
    config->has_main_output = true;

    switch (index) {
    case 0:
        config->id = GAIN__AUDIO_PORT_CONFIG_MONO;
        std::strcpy(config->name, "Mono");
        config->main_input_channel_count = 1;
        config->main_input_port_type = CLAP_PORT_MONO;
        config->main_output_channel_count = 1;
        config->main_output_port_type = CLAP_PORT_MONO;
        return true;
    case 1:
        config->id = GAIN__AUDIO_PORT_CONFIG_STEREO;
        std::strcpy(config->name, "Stereo");
        config->main_input_channel_count = 2;
        config->main_input_port_type = CLAP_PORT_STEREO;
        config->main_output_channel_count = 2;
        config->main_output_port_type = CLAP_PORT_STEREO;
        return true;
    default:
        return false;
    }
}

bool gain__audio_ports_config__select(const clap_plugin_t *plugin, clap_id config_id)
{
    switch (config_id) {
    case GAIN__AUDIO_PORT_CONFIG_MONO:
    case GAIN__AUDIO_PORT_CONFIG_STEREO:
    {
        gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
        priv->audio_port_config_id = config_id;
        return true;
    }
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
uint32_t gain__params__count(const clap_plugin_t *plugin)
{
    (void)plugin;
    return GAIN__PARAM__COUNT;
}

bool gain__params__get_info(
    const clap_plugin_t *plugin, uint32_t param_index,
    clap_param_info_t *param_info)
{
    (void)plugin;

    static const gain__param_values_t defaults;

    switch (param_index) {
    case GAIN__PARAM_GAIN:
        param_info->id = 0;
        param_info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        param_info->cookie = nullptr;
        std::strcpy(param_info->name, "Gain");
        std::strcpy(param_info->module, "");
        param_info->min_value = 0.0;
        param_info->max_value = 10.0;
        param_info->default_value = defaults.gain;
        return true;
    default:
        return false;
    }
}

bool gain__params__get_value(
    const clap_plugin_t *plugin, clap_id param_id, double *value)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    switch (param_id) {
    case GAIN__PARAM_GAIN:
        *value = priv->param.gain;
        return true;
    default:
        return false;
    }
}

bool gain__params__value_to_text(
    const clap_plugin_t *plugin, clap_id param_id,
    double value, char *display, uint32_t size)
{
    (void)plugin;

    switch (param_id) {
    case GAIN__PARAM_GAIN:
        std::snprintf(display, size, "%f dB", 20.0 * std::log10(value));
        return true;
    default:
        return false;
    }
}

bool gain__params__text_to_value(
    const clap_plugin_t *plugin, clap_id param_id,
    const char *display, double *value)
{
    (void)plugin;

    switch (param_id) {
    case GAIN__PARAM_GAIN:
        if (std::sscanf(display, "%lf", value) != 1)
            return false;
        return true;
    default:
        return false;
    }
}

void gain__params__flush(
    const clap_plugin_t *plugin,
    const clap_input_events_t *in, const clap_output_events_t *out)
{
    (void)out;

    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    uint32_t size = in->size(in);

    for (uint32_t i = 0; i < size; ++i) {
        const clap_event_header_t *hdr = in->get(in, i);
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const clap_event_param_value *pv = (const clap_event_param_value *)hdr;
            switch (pv->param_id) {
            case GAIN__PARAM_GAIN:
                priv->param.gain = pv->value;
                break;
            }
        }
    }
}

//------------------------------------------------------------------------------
bool gain__state__save(const clap_plugin_t *plugin, const clap_ostream_t *stream)
{
    const gain__private_t *priv = (const gain__private_t *)plugin->plugin_data;
    const gain__param_values_t *pv = &priv->param;

    if (stream->write(stream, &pv->gain, sizeof(pv->gain)) != sizeof(pv->gain))
        return false;

    return true;
}

bool gain__state__load(const clap_plugin_t *plugin, const clap_istream_t *stream)
{
    gain__param_values_t pv{};
    if (stream->read(stream, &pv.gain, sizeof(pv.gain)) != sizeof(pv.gain))
        return false;

    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    priv->param = pv;
    return true;
}

//------------------------------------------------------------------------------
static void on_imgui_frame(void *self_)
{
    const clap_plugin_t *plugin = (const clap_plugin_t *)self_;
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    #pragma message("TODO: the user interface")
    ImGui::SetNextWindowSize(ImVec2{400, 100});
    if (ImGui::Begin("Gain")) {
        static std::string txt{"Edit me"};
        ImGui::InputText("What's your name?", &txt);
        static float val = 0.5f;
        ImGui::SliderFloat("Value", &val, 0.0f, 1.0f);
    }
    ImGui::End();
}

//------------------------------------------------------------------------------
static constexpr uint32_t gui_update_interval = 20;

static constexpr uint32_t gui_default_width = 800;
static constexpr uint32_t gui_default_height = 600;

#if defined(_WIN32)
static const char *native_window_api = CLAP_WINDOW_API_WIN32;
static intptr_t get_native_window_id(const clap_window_t *w) { return (intptr_t)w->win32; }
#elif defined(__APPLE__)
static const char *native_window_api = CLAP_WINDOW_API_COCOA;
static intptr_t get_native_window_id(const clap_window_t *w) { return (intptr_t)w->cocoa; }
#else
static const char *native_window_api = CLAP_WINDOW_API_X11;
static intptr_t get_native_window_id(const clap_window_t *w) { return (intptr_t)w->x11; }
#endif

static bool install_gui_timer(const clap_plugin_t *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    clap_id timer_id = priv->gui_timer_id;
    if (timer_id != CLAP_INVALID_ID)
        return true;

    const clap_host_t *host = priv->host;
    const clap_host_timer_support_t *timer_support = priv->host_ext.timer_support;
    if (!timer_support || !timer_support->register_timer(host, gui_update_interval, &timer_id))
        return false;

    priv->gui_timer_id = timer_id;
    return true;
}

static bool uninstall_gui_timer(const clap_plugin_t *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    clap_id timer_id = priv->gui_timer_id;
    if (timer_id == CLAP_INVALID_ID)
        return true;

    const clap_host_t *host = priv->host;
    const clap_host_timer_support_t *timer_support = priv->host_ext.timer_support;
    if (!timer_support || !timer_support->unregister_timer(host, timer_id))
        return false;

    priv->gui_timer_id = CLAP_INVALID_ID;
    return true;
}

bool gain__gui_is_api_supported(const clap_plugin_t *plugin, const char *api, bool is_floating)
{
    (void)plugin;
    (void)is_floating;
    if (!std::strcmp(api, native_window_api))
        return true;
    return false;
}

bool gain__gui_get_preferred_api(const clap_plugin_t *plugin, const char **api, bool *is_floating)
{
    (void)plugin;
    *api = native_window_api;
    *is_floating = false;
    return true;
}

bool gain__gui_create(const clap_plugin_t *plugin, const char *api, bool is_floating)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (priv->window)
        return false;

    if (std::strcmp(api, native_window_api))
        return false;

    bool init_ok = false;
    ct_imgui_window *window = new ct_imgui_window{plugin->desc->id, gui_default_width, gui_default_height, &init_ok};
    if (!init_ok) {
        delete window;
        return false;
    }
    priv->window.reset(window);

    window->m_floating = is_floating;

    window->m_imgui_callback = &on_imgui_frame;
    window->m_imgui_callback_data = (void *)plugin;

    return true;
}

void gain__gui_destroy(const clap_plugin_t *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (!uninstall_gui_timer(plugin)) {
        // error
    }

    if (!priv->window)
        return;

    priv->window.reset();
}

bool gain__gui_set_scale(const clap_plugin_t *plugin, double scale)
{
    (void)plugin;

    if (scale != 1.0)
        return false;

    return true;
}

bool gain__gui_get_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    ct_imgui_window *window = priv->window.get();

    if (!window)
        return false;

    return window->get_size(width, height);
}

bool gain__gui_can_resize(const clap_plugin_t *plugin)
{
    (void)plugin;
    return true;
}

bool gain__gui_get_resize_hints(const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints)
{
    (void)plugin;
    (void)hints;
    return false;
}

bool gain__gui_adjust_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)
{
    (void)plugin;
    (void)width;
    (void)height;
    return true;
}

bool gain__gui_set_size(const clap_plugin_t *plugin, uint32_t width, uint32_t height)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    ct_imgui_window *window = priv->window.get();

    if (!window)
        return false;

    return window->set_size(width, height);
}

bool gain__gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window_)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    ct_imgui_window *window = priv->window.get();

    if (!window || window->m_floating)
        return false;

    if (std::strcmp(window_->api, native_window_api))
        return false;

    window->set_parent(get_native_window_id(window_));
    return false;
}

bool gain__gui_set_transient(const clap_plugin_t *plugin, const clap_window_t *window_)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    ct_imgui_window *window = priv->window.get();

    if (!window || !window->m_floating)
        return false;

    if (std::strcmp(window_->api, native_window_api))
        return false;

    window->set_transient(get_native_window_id(window_));
    return false;
}

void gain__gui_suggest_title(const clap_plugin_t *plugin, const char *title)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    ct_imgui_window *window = priv->window.get();

    if (!window)
        return;

    window->set_caption(title);
}

bool gain__gui_show(const clap_plugin_t *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    ct_imgui_window *window = priv->window.get();

    if (!window)
        return false;

    if (!window->show())
        return false;

    if (!install_gui_timer(plugin)) {
        // error
    }

    ct_imgui_window::update(0);

    return true;
}

bool gain__gui_hide(const clap_plugin_t *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    ct_imgui_window *window = priv->window.get();

    if (!window)
        return false;

    if (!window->hide())
        return false;

    if (!uninstall_gui_timer(plugin)) {
        // error
    }

    return true;
}

//------------------------------------------------------------------------------
static void gain__timer_support_on_timer(const clap_plugin_t *plugin, clap_id timer_id)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (timer_id == priv->gui_timer_id) {
        ct_imgui_window::update(0);
        priv->window->post_redisplay();
    }
}

//------------------------------------------------------------------------------
static const void *gain__get_extension(
    const clap_plugin_t *plugin, const char *id)
{
    (void)plugin;

    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) {
        static const clap_plugin_audio_ports_t ext = {
            &gain__audio_ports__count,
            &gain__audio_ports__get,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS_CONFIG)) {
        static const clap_plugin_audio_ports_config_t ext = {
            &gain__audio_ports_config__count,
            &gain__audio_ports_config__get,
            &gain__audio_ports_config__select,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_PARAMS)) {
        static const clap_plugin_params_t ext = {
            &gain__params__count,
            &gain__params__get_info,
            &gain__params__get_value,
            &gain__params__value_to_text,
            &gain__params__text_to_value,
            &gain__params__flush,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_STATE)) {
        static const clap_plugin_state_t ext = {
            &gain__state__save,
            &gain__state__load,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_GUI)) {
        static const clap_plugin_gui_t ext = {
            &gain__gui_is_api_supported,
            &gain__gui_get_preferred_api,
            &gain__gui_create,
            &gain__gui_destroy,
            &gain__gui_set_scale,
            &gain__gui_get_size,
            &gain__gui_can_resize,
            &gain__gui_get_resize_hints,
            &gain__gui_adjust_size,
            &gain__gui_set_size,
            &gain__gui_set_parent,
            &gain__gui_set_transient,
            &gain__gui_suggest_title,
            &gain__gui_show,
            &gain__gui_hide,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)) {
        static const clap_plugin_timer_support ext = {
            &gain__timer_support_on_timer,
        };
        return &ext;
    }
    return nullptr;
}

static void gain__on_main_thread(const clap_plugin_t *plugin)
{
    (void)plugin;
}

//------------------------------------------------------------------------------
static uint32_t factory__get_plugin_count(const clap_plugin_factory_t *factory)
{
    (void)factory;
    return 1;
}

static const clap_plugin_descriptor_t *factory__get_plugin_descriptor(
    const clap_plugin_factory_t *factory, uint32_t index)
{
    (void)factory;

    if (index == 0) {
        static const char *features[] =
        {
            "audio_effect",
            nullptr,
        };
        static const clap_plugin_descriptor_t desc =
        {
            CLAP_VERSION,
            "com.example.gain",
            "Gain",
            "Examples",
            "https://example.com/products/Gain/",
            "https://example.com/manuals/Gain/",
            "https://example.com/support/",
            "1.0",
            "An example audio plugin",
            features,
        };
        return &desc;
    }

    return nullptr;
}

static const clap_plugin_t *factory__create_plugin(
    const clap_plugin_factory_t *factory, const clap_host_t *host,
    const char *plugin_id)
{
    const clap_plugin_descriptor_t *desc = factory__get_plugin_descriptor(factory, 0);

    if (!std::strcmp(plugin_id, desc->id)) {
        std::unique_ptr<clap_plugin_t> plugin{new clap_plugin_t};
        plugin->desc = desc;
        plugin->init = &gain__init;
        plugin->destroy = &gain__destroy;
        plugin->activate = &gain__activate;
        plugin->deactivate = &gain__deactivate;
        plugin->start_processing = &gain__start_processing;
        plugin->stop_processing = &gain__stop_processing;
        plugin->reset = &gain__reset;
        plugin->process = &gain__process;
        plugin->get_extension = &gain__get_extension;
        plugin->on_main_thread = &gain__on_main_thread;
        gain__private_t *priv = new gain__private_t;
        plugin->plugin_data = priv;
        priv->host = host;
        return plugin.release();
    }

    return nullptr;
}

//------------------------------------------------------------------------------
static bool plugin__init(const char *plugin_path)
{
    (void)plugin_path;
    return true;
}

static void plugin__deinit()
{
}

static const void *plugin__get_factory(const char *factory_id)
{
    if (!std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID))
    {
        static const clap_plugin_factory_t plugin_factory =
        {
            &factory__get_plugin_count,
            &factory__get_plugin_descriptor,
            &factory__create_plugin,
        };
        return &plugin_factory;
    }

    return nullptr;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry =
{
    CLAP_VERSION,
    &plugin__init,
    &plugin__deinit,
    &plugin__get_factory,
};
