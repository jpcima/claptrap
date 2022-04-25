#include "vstgui_init.hpp"
#include "include_vstgui.hpp"
#include "apple_bundle.hpp"
VSTGUI_WARNINGS_PUSH
#include <vstgui/lib/vstguiinit.h>
VSTGUI_WARNINGS_POP
#if defined(_WIN32)
#   include <windows.h>
#elif defined(__APPLE__)
#   include <CoreFoundation/CoreFoundation.h>
#else
#   include <dlfcn.h>
#endif

bool vstgui_init(const char *plugin_path)
{
#if defined(_WIN32)
    HINSTANCE instance = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)(void *)&vstgui_init, &instance))
    {
        return false;
    }

    VSTGUI::PlatformInstanceHandle platform_handle = instance;
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
    VSTGUI::PlatformInstanceHandle platform_handle = bundle;
#else
    struct dl_handle_delete {
        void operator()(void *x) const noexcept { dlclose(x); }
    };
    using dl_handle_ptr = std::unique_ptr<void, dl_handle_delete>;

    dl_handle_ptr dlh{dlopen(plugin_path, RTLD_LAZY|RTLD_NOLOAD)};
    if (!dlh)
        return false;

    VSTGUI::PlatformInstanceHandle platform_handle = dlh.get();
#endif

    VSTGUI::init(platform_handle);
    return true;
}

void vstgui_exit()
{
    VSTGUI::exit();
}
