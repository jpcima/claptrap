#pragma once

#if defined(FMT_NAMESPACE)
#   define fmt FMT_NAMESPACE
#endif
#include <fmt/format.h>
#if defined(FMT_NAMESPACE)
#   undef fmt
#endif
namespace ct { namespace fmt {
#if defined(FMT_NAMESPACE)
using namespace ::FMT_NAMESPACE;
#else
using namespace ::fmt;
#endif
} }
