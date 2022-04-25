#include "ct_plugin_factory.hpp"
#include "ct_defs.hpp"
#include "unicode_helpers.hpp"
#include <travesty/base.h>
#include <clap/clap.h>
#include <nonstd/scope.hpp>
#include <memory>
#if defined(_WIN32)
#   include <windows.h>
#elif defined(__APPLE__)
#   include <CoreFoundation/CoreFoundation.h>
#else
#   include <dlfcn.h>
#   include <link.h>
#endif

#if !defined(CT_EXPORT)
#   if defined(_WIN32)
#       define CT_EXPORT __declspec(dllexport)
#   elif defined(__GNUC__)
#       define CT_EXPORT __attribute__((visibility("default")))
#   else
#       define CT_EXPORT
#   endif
#endif

#if defined(_WIN32)
HINSTANCE g_instance;
#endif

//------------------------------------------------------------------------------
static bool g_module_initialized = false;

extern "C" CT_EXPORT void *V3_API GetPluginFactory()
{
    LOG_PLUGIN_CALL;

    static const clap_plugin_factory_t *cf = (const clap_plugin_factory_t *)clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!cf)
        return nullptr;

    static ct_plugin_factory instance{cf};
    return &instance;
}

#if defined(_WIN32)
extern "C" CT_EXPORT bool InitDll()
{
    LOG_PLUGIN_CALL;

    if (g_module_initialized)
        return true;

    std::unique_ptr<char16_t[]> path{new char16_t[32768]};
    if (GetModuleFileNameW(g_instance, (wchar_t *)path.get(), 32768) == 0)
        return false;

    if (!clap_entry.init(UTF_convert<char>(path.get()).c_str()))
        return false;

    g_module_initialized = true;
    return true;
}

extern "C" CT_EXPORT bool ExitDll()
{
    LOG_PLUGIN_CALL;

    if (!g_module_initialized)
        return true;

    clap_entry.deinit();
    g_module_initialized = false;
    return true;
}
#elif defined(__APPLE__)
extern "C" CT_EXPORT bool bundleEntry(CFBundleRef bundle)
{
    LOG_PLUGIN_CALL;

    (void)bundle;

    if (g_module_initialized)
        return true;

    CFURLRef url = CFBundleCopyExecutableURL(bundle);
    if (!url)
        return false;
    auto url_cleanup = nonstd::make_scope_exit([url]() { CFRelease(url); });

    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (!path)
        return false;
    auto path_cleanup = nonstd::make_scope_exit([path]() { CFRelease(path); });

    ///
    auto get_string_from_cfstring = [](CFStringRef str, std::string *result) -> bool {
        const char *chars8 = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
        if (chars8) {
            result->assign(chars8);
            return true;
        }
        CFIndex length = CFStringGetLength(str);
        CFIndex bytes = 0;
        if (CFStringGetBytes(str, CFRange{0, length}, kCFStringEncodingUTF8, 0, false, nullptr, 0, &bytes) < 1)
            return false;
        result->resize(bytes);
        CFStringGetBytes(str, CFRange{0, length}, kCFStringEncodingUTF8, 0, false, (uint8_t *)result->data(), bytes, nullptr);
        return true;
    };

    ///
    std::string path8;
    if (!get_string_from_cfstring(path, &path8))
        return false;

    if (!clap_entry.init(path8.c_str()))
        return false;

    g_module_initialized = true;
    return true;
}

extern "C" CT_EXPORT bool bundleExit()
{
    LOG_PLUGIN_CALL;

    if (!g_module_initialized)
        return true;

    clap_entry.deinit();
    g_module_initialized = false;
    return true;
}
#else
extern "C" CT_EXPORT bool ModuleEntry(void *mod)
{
    LOG_PLUGIN_CALL;

    if (g_module_initialized)
        return true;

    const struct link_map *link_map = 0;
    if (dlinfo(mod, RTLD_DI_LINKMAP, &link_map) == -1)
        return false;

    const char *module_path = link_map->l_name;

    if (!clap_entry.init(module_path))
        return false;

    g_module_initialized = true;
    return true;
}

extern "C" CT_EXPORT bool ModuleExit()
{
    LOG_PLUGIN_CALL;

    if (!g_module_initialized)
        return true;

    clap_entry.deinit();
    g_module_initialized = false;
    return true;
}
#endif

#if defined(_WIN32)
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
        g_instance = instance;

    return TRUE;
}
#endif
