#pragma once
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>

#define CT_MESSAGE_PREFIX        "[ct] "
#define CT_MESSAGE_PREFIX_SPACES "     "

#define CT_WARNING(format, ...) do {            \
        CT_MESSAGE("Warning: ", ##__VA_ARGS__); \
        std::abort();                           \
    } while (0)

#define CT_FATAL(format, ...) do {              \
        CT_MESSAGE("Fatal: ", ##__VA_ARGS__);   \
        std::abort();                           \
    } while (0)

#define CT_MESSAGE(...)                             \
    CT_MESSAGE_NP(CT_MESSAGE_PREFIX, ##__VA_ARGS__)

#define CT_MESSAGE_NP(...) do {                 \
        CT_LOG_PRINT(__VA_ARGS__, '\n');        \
        CT_LOG_FLUSH();                         \
    } while (0)

#define CT_LOG_PRINT(...)                       \
    ct::ct__message(std::cerr, ##__VA_ARGS__)

#define CT_LOG_FLUSH()                          \
    std::cerr.flush()

//------------------------------------------------------------------------------
namespace ct {

template <class... Args>
std::string ct__format(Args &&... args)
{
    std::ostringstream buf;
    (buf << ... << std::forward<Args>(args));
    return buf.str();
}

template <class... Args>
void ct__message(std::ostream &out, Args &&... args)
{
    const std::string text = ct__format(std::forward<Args>(args)...);
    out.write(text.data(), text.size());
}

} // namespace ct
