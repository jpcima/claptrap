#pragma once
#include "libs/fmt.hpp"
#include <cstdio>
#include <cstdlib>

#define CT_MESSAGE_PREFIX        "[ct] "
#define CT_MESSAGE_PREFIX_SPACES "     "

#define CT_WARNING(format, ...) do {                    \
        CT_MESSAGE("Warning: " format, ##__VA_ARGS__);  \
        std::abort();                                   \
    } while (0)

#define CT_FATAL(format, ...) do {                      \
        CT_MESSAGE("Fatal: " format, ##__VA_ARGS__);    \
        std::abort();                                   \
    } while (0)

#define CT_MESSAGE(format, ...)                         \
    CT_MESSAGE_NP(CT_MESSAGE_PREFIX format, ##__VA_ARGS__)

#define CT_MESSAGE_NP(format, ...) do {             \
        CT_LOG_PRINTF(format "\n", ##__VA_ARGS__);  \
        CT_LOG_FLUSH();                             \
    } while (0)

#define CT_LOG_PRINTF(format, ...)                  \
    ::ct::fmt::print(stderr, format, ##__VA_ARGS__)

#define CT_LOG_FLUSH()                          \
    std::fflush(stderr)
