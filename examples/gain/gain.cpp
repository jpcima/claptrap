#include "common/include_vstgui.hpp"
#include "common/vstgui_x11_runloop.hpp"
#include "common/apple_bundle.hpp"
#include <clap/clap.h>
VSTGUI_WARNINGS_PUSH
#include <vstgui/uidescription/uidescription.h>
#include <vstgui/lib/vstguiinit.h>
VSTGUI_WARNINGS_POP
#if defined(_WIN32)
#   include <windows.h>
#elif defined(__APPLE__)
#   include <CoreFoundation/CoreFoundation.h>
#else
#   include <dlfcn.h>
#endif
#include <memory>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cassert>
using namespace VSTGUI;

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
    const clap_host *host = nullptr;

    //--------------------------------------------------------------------------
    struct {
        const clap_host_timer_support *timer_support;
    } host_ext;

    //--------------------------------------------------------------------------
    clap_id audio_port_config_id = 0;
    gain__param_values_t param = {};

    //--------------------------------------------------------------------------
    SharedPointer<CFrame> frame;
    std::unique_ptr<IPlatformFrameConfig> frame_config;
    bool frame_is_open = false;
    bool frame_floating = false;
    intptr_t frame_parent = 0;
    intptr_t frame_transient = 0;
};

static bool gain__init(const clap_plugin *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    const clap_host *host = priv->host;
    priv->host_ext.timer_support = (const clap_host_timer_support *)host->get_extension(host, CLAP_EXT_TIMER_SUPPORT);
    return true;
}

static void gain__destroy(const clap_plugin *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    delete priv;
    delete plugin;
}

static bool gain__activate(
    const clap_plugin *plugin, double sample_rate,
    uint32_t min_frames_count, uint32_t max_frames_count)
{
    (void)plugin;
    (void)sample_rate;
    (void)min_frames_count;
    (void)max_frames_count;
    return true;
}

static void gain__deactivate(const clap_plugin *plugin)
{
    (void)plugin;
}

static bool gain__start_processing(const clap_plugin *plugin)
{
    (void)plugin;
    return true;
}

static void gain__stop_processing(const clap_plugin *plugin)
{
    (void)plugin;
}

static void gain__reset(const clap_plugin *plugin)
{
    (void)plugin;
}

static clap_process_status gain__process(
    const clap_plugin *plugin, const clap_process *process)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    uint32_t nf = process->frames_count;
    if (nf < 1)
        return CLAP_PROCESS_CONTINUE;

    clap_audio_buffer in = process->audio_inputs[0];
    clap_audio_buffer out = process->audio_outputs[0];
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
static uint32_t gain__audio_ports__count(const clap_plugin *plugin, bool is_input)
{
    (void)plugin;
    (void)is_input;
    return is_input ? (uint32_t)GAIN__NUM_INPUTS : (uint32_t)GAIN__NUM_OUTPUTS;
}

static bool gain__audio_ports__get(
    const clap_plugin *plugin, uint32_t index, bool is_input,
    clap_audio_port_info *info)
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

uint32_t gain__audio_ports_config__count(const clap_plugin *plugin)
{
    (void)plugin;
    return GAIN__AUDIO_PORT_CONFIG__COUNT;
}

bool gain__audio_ports_config__get(const clap_plugin *plugin, uint32_t index, clap_audio_ports_config *config)
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

bool gain__audio_ports_config__select(const clap_plugin *plugin, clap_id config_id)
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
uint32_t gain__params__count(const clap_plugin *plugin)
{
    (void)plugin;
    return GAIN__PARAM__COUNT;
}

bool gain__params__get_info(
    const clap_plugin *plugin, uint32_t param_index,
    clap_param_info *param_info)
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
    const clap_plugin *plugin, clap_id param_id, double *value)
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
    const clap_plugin *plugin, clap_id param_id,
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
    const clap_plugin *plugin, clap_id param_id,
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
    const clap_plugin *plugin,
    const clap_input_events *in, const clap_output_events *out)
{
    (void)out;

    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    uint32_t size = in->size(in);

    for (uint32_t i = 0; i < size; ++i) {
        const clap_event_header *hdr = in->get(in, i);
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
bool gain__state__save(const clap_plugin *plugin, const clap_ostream *stream)
{
    const gain__private_t *priv = (const gain__private_t *)plugin->plugin_data;
    const gain__param_values_t *pv = &priv->param;

    if (stream->write(stream, &pv->gain, sizeof(pv->gain)) != sizeof(pv->gain))
        return false;

    return true;
}

bool gain__state__load(const clap_plugin *plugin, const clap_istream *stream)
{
    gain__param_values_t pv{};
    if (stream->read(stream, &pv.gain, sizeof(pv.gain)) != sizeof(pv.gain))
        return false;

    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    priv->param = pv;
    return true;
}

//------------------------------------------------------------------------------
static constexpr uint32_t gui_default_width = 800;
static constexpr uint32_t gui_default_height = 600;

#if defined(_WIN32)
static const char *native_window_api = CLAP_WINDOW_API_WIN32;
static intptr_t get_native_window_id(const clap_window *w) { return (intptr_t)w->win32; }
#elif defined(__APPLE__)
static const char *native_window_api = CLAP_WINDOW_API_COCOA;
static intptr_t get_native_window_id(const clap_window *w) { return (intptr_t)w->cocoa; }
#else
static const char *native_window_api = CLAP_WINDOW_API_X11;
static intptr_t get_native_window_id(const clap_window *w) { return (intptr_t)w->x11; }
#endif

bool gain__gui_is_api_supported(const clap_plugin *plugin, const char *api, bool is_floating)
{
    (void)plugin;
    (void)is_floating;
    if (!std::strcmp(api, native_window_api))
        return true;
    return false;
}

bool gain__gui_get_preferred_api(const clap_plugin *plugin, const char **api, bool *is_floating)
{
    (void)plugin;
    *api = native_window_api;
    *is_floating = false;
    return true;
}

bool gain__gui_create(const clap_plugin *plugin, const char *api, bool is_floating)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (priv->frame)
        return false;

    if (std::strcmp(api, native_window_api))
        return false;

    CRect size{0, 0, gui_default_width, gui_default_height};
    VSTGUIEditorInterface *ied = nullptr;

    SharedPointer<CFrame> frame = makeOwned<CFrame>(size, ied);

    priv->frame = frame;
    priv->frame_floating = is_floating;

    UIDescription desc{"gain.uidesc"};
    if (!desc.parse())
        return false;

    IController *ctrl = nullptr;
    SharedPointer<CView> view = owned(desc.createView("view", ctrl));
    if (!view)
        return false;

    frame->addView(view);
    view->remember();

    return true;
}

void gain__gui_destroy(const clap_plugin *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (!priv->frame)
        return;

    priv->frame = nullptr;
}

bool gain__gui_set_scale(const clap_plugin *plugin, double scale)
{
    (void)plugin;

    if (scale != 1.0)
        return false;

    return true;
}

bool gain__gui_get_size(const clap_plugin *plugin, uint32_t *width, uint32_t *height)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    CFrame *frame = priv->frame.get();

    if (!frame)
        return false;

    CRect rect = frame->getViewSize();
    *width = (uint32_t)rect.getWidth();
    *height = (uint32_t)rect.getHeight();
    return true;
}

bool gain__gui_can_resize(const clap_plugin *plugin)
{
    (void)plugin;
    return true;
}

bool gain__gui_get_resize_hints(const clap_plugin *plugin, clap_gui_resize_hints *hints)
{
    (void)plugin;
    (void)hints;
    return false;
}

bool gain__gui_adjust_size(const clap_plugin *plugin, uint32_t *width, uint32_t *height)
{
    (void)plugin;
    (void)width;
    (void)height;
    return true;
}

bool gain__gui_set_size(const clap_plugin *plugin, uint32_t width, uint32_t height)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    CFrame *frame = priv->frame.get();

    if (!frame)
        return false;

    frame->setSize((CCoord)width, (CCoord)height);
    return true;
}

bool gain__gui_set_parent(const clap_plugin *plugin, const clap_window *window_)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (std::strcmp(window_->api, native_window_api))
        return false;

    priv->frame_parent = get_native_window_id(window_);
    return true;
}

bool gain__gui_set_transient(const clap_plugin *plugin, const clap_window *window_)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    if (std::strcmp(window_->api, native_window_api))
        return false;

    priv->frame_transient = get_native_window_id(window_);
    return false;
}

void gain__gui_suggest_title(const clap_plugin *plugin, const char *title)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    CFrame *frame = priv->frame.get();

    if (!frame)
        return;

    //TODO
    (void)title;
}

bool gain__gui_show(const clap_plugin *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    CFrame *frame = priv->frame.get();

    if (!frame)
        return false;

    if (!priv->frame_is_open) {
        void *system_window = nullptr;

        if (!priv->frame_floating)
            system_window = (void *)priv->frame_parent;

#if defined(_WIN32) || defined(__APPLE__)
        IPlatformFrameConfig *frame_config = nullptr;
#else
        X11::FrameConfig *frame_config = static_cast<X11::FrameConfig *>(priv->frame_config.get());
        if (!frame_config) {
            frame_config = new X11::FrameConfig;
            priv->frame_config.reset(frame_config);
            frame_config->runLoop = owned(new_vstgui_x11_runloop(priv->host));
        }
#endif

        if (!frame->open(system_window, PlatformType::kDefaultNative, frame_config))
            return false;

        priv->frame_is_open = true;
    }

    frame->setVisible(true);
    return true;
}

bool gain__gui_hide(const clap_plugin *plugin)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;
    CFrame *frame = priv->frame.get();

    if (!frame)
        return false;

    frame->setVisible(false);
    return true;
}

//------------------------------------------------------------------------------
#if !defined(_WIN32) && !defined(__APPLE__)
static void gain__posix_fd_support_on_fd(const clap_plugin *plugin, int fd, int flags)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    (void)flags;

    X11::FrameConfig *frame_config = static_cast<X11::FrameConfig *>(priv->frame_config.get());
    if (frame_config)
        update_x11_runloop_on_fd(frame_config->runLoop.get(), fd);
}
#endif

//------------------------------------------------------------------------------
static void gain__timer_support_on_timer(const clap_plugin *plugin, clap_id timer_id)
{
    gain__private_t *priv = (gain__private_t *)plugin->plugin_data;

    (void)priv;
    (void)timer_id;

    // if (timer_id == ) {
    // }
    // else
    {
#if !defined(_WIN32) && !defined(__APPLE__)
        X11::FrameConfig *frame_config = static_cast<X11::FrameConfig *>(priv->frame_config.get());
        if (frame_config)
            update_x11_runloop_on_timer(frame_config->runLoop.get(), timer_id);
#endif
    }
}

//------------------------------------------------------------------------------
static const void *gain__get_extension(
    const clap_plugin *plugin, const char *id)
{
    (void)plugin;

    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) {
        static const clap_plugin_audio_ports ext = {
            &gain__audio_ports__count,
            &gain__audio_ports__get,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS_CONFIG)) {
        static const clap_plugin_audio_ports_config ext = {
            &gain__audio_ports_config__count,
            &gain__audio_ports_config__get,
            &gain__audio_ports_config__select,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_PARAMS)) {
        static const clap_plugin_params ext = {
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
        static const clap_plugin_state ext = {
            &gain__state__save,
            &gain__state__load,
        };
        return &ext;
    }
    if (!std::strcmp(id, CLAP_EXT_GUI)) {
        static const clap_plugin_gui ext = {
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
#if !defined(_WIN32) && !defined(__APPLE__)
    if (!std::strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT)) {
        static const clap_plugin_posix_fd_support ext = {
            &gain__posix_fd_support_on_fd,
        };
        return &ext;
    }
#endif
    if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)) {
        static const clap_plugin_timer_support ext = {
            &gain__timer_support_on_timer,
        };
        return &ext;
    }
    return nullptr;
}

static void gain__on_main_thread(const clap_plugin *plugin)
{
    (void)plugin;
}

//------------------------------------------------------------------------------
static uint32_t factory__get_plugin_count(const clap_plugin_factory *factory)
{
    (void)factory;
    return 1;
}

static const clap_plugin_descriptor *factory__get_plugin_descriptor(
    const clap_plugin_factory *factory, uint32_t index)
{
    (void)factory;

    if (index == 0) {
        static const char *features[] =
        {
            "audio_effect",
            nullptr,
        };
        static const clap_plugin_descriptor desc =
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

static const clap_plugin *factory__create_plugin(
    const clap_plugin_factory *factory, const clap_host *host,
    const char *plugin_id)
{
    const clap_plugin_descriptor *desc = factory__get_plugin_descriptor(factory, 0);

    if (!std::strcmp(plugin_id, desc->id)) {
        std::unique_ptr<clap_plugin> plugin{new clap_plugin};
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

#if defined(_WIN32)
    HINSTANCE instance = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)(void *)&plugin__init, &instance))
    {
        return false;
    }
    VSTGUI::init(instance);
#elif defined(__APPLE__)
    struct cfobject_cleanup {
        explicit cfobject_cleanup(CFTypeRef obj) : m_obj{obj} {}
        ~cfobject_cleanup() { if (m_obj) CFRelease(m_obj); }
        CFTypeRef m_obj = nullptr;
    };

    CFBundleRef bundle = create_bundle_from_plugin_path(plugin_path);
    if (!bundle)
        return false;

    cfobject_cleanup cleanup{bundle};
    VSTGUI::init(bundle);
#else
    struct dl_handle_delete {
        void operator()(void *x) const noexcept { dlclose(x); }
    };
    using dl_handle_ptr = std::unique_ptr<void, dl_handle_delete>;

    dl_handle_ptr dlh{dlopen(plugin_path, RTLD_LAZY|RTLD_NOLOAD)};
    if (!dlh)
        return false;

    VSTGUI::init(dlh.get());
#endif

    return true;
}

static void plugin__deinit()
{
}

static const void *plugin__get_factory(const char *factory_id)
{
    if (!std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID))
    {
        static const clap_plugin_factory plugin_factory =
        {
            &factory__get_plugin_count,
            &factory__get_plugin_descriptor,
            &factory__create_plugin,
        };
        return &plugin_factory;
    }

    return nullptr;
}

CLAP_EXPORT const clap_plugin_entry clap_entry =
{
    CLAP_VERSION,
    &plugin__init,
    &plugin__deinit,
    &plugin__get_factory,
};
