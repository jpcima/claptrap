#pragma once
#include "ct_messages.hpp"

#if !defined(CT_UNLIKELY)
#   if defined(__GNUC__)
#       define CT_UNLIKELY(x) __builtin_expect((x), 0)
#   else
#       define CT_UNLIKELY(x)
#   endif
#endif

#if defined(CT_ASSERTIONS) || !defined(NDEBUG)
#define CT_ASSERT(x) do { if (CT_UNLIKELY(!(x))) {                    \
        CT_FATAL("Assertion failed ({}) at {}:{}\n",                  \
                 #x, __FILE__, __LINE__);                             \
    } } while (0)
#else
#define CT_ASSERT(x)
#endif
