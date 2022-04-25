#include "apple_bundle.hpp"
#include <cstring>

#if defined(__APPLE__)

CFBundleRef create_bundle_from_plugin_path(const char *path)
{
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(CFAllocatorGetDefault(), (const UInt8 *)path, std::strlen(path), false);
    if (!url)
        return nullptr;

    for (;;) {
        CFBundleRef bundle = CFBundleCreate(CFAllocatorGetDefault(), url);
        if (bundle) {
            if (CFBundlePreflightExecutable(bundle, nullptr)) {
                CFRelease(url);
                return bundle;
            }
            CFRelease(bundle);
        }
        CFURLRef prev_url = url;
        bool got_url = CFURLCopyResourcePropertyForKey(prev_url, kCFURLParentDirectoryURLKey, &url, nullptr) && url != nullptr;
        CFRelease(prev_url);
        if (!got_url)
            return nullptr;
    }
}

#endif // defined(__APPLE__)
