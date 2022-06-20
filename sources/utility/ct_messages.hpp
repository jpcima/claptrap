#pragma once
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>

#define CT_MESSAGE_PREFIX        "[ct] "
#define CT_MESSAGE_PREFIX_SPACES "     "

#define CT_WARNING(format, ...)                 \
    CT_MESSAGE("Warning: ", ##__VA_ARGS__);

#define CT_FATAL(format, ...) do {              \
        CT_MESSAGE("Fatal: ", ##__VA_ARGS__);   \
        std::abort();                           \
    } while (0)

#define CT_MESSAGE(...)                             \
    CT_MESSAGE_NP(CT_MESSAGE_PREFIX, ##__VA_ARGS__)

#define CT_MESSAGE_NP(...)                      \
    CT_LOG_PRINT(__VA_ARGS__, '\n');

#define CT_LOG_PRINT(...)                       \
    ::ct::log_printer().print(__VA_ARGS__)

//------------------------------------------------------------------------------
namespace ct {

class message_printer {
public:
    explicit message_printer(std::ostream *stream = nullptr)
        : m_stream{stream}
    {
    }

    message_printer(message_printer &&other)
        : m_stream{other.m_stream}, m_buf{std::move(other.m_buf)}
    {
        other.m_stream = nullptr;
    }

    ~message_printer()
    {
        if (m_stream) {
            const std::string str = m_buf.str();
            m_buf = std::ostringstream{};
            m_stream->write(str.data(), str.size());
            m_stream->flush();
        }
    }

    template <class... Args> void print(Args &&... args)
    {
        (m_buf << ... << std::forward<Args>(args));
    }

private:
    std::ostream *m_stream = nullptr;
    std::ostringstream m_buf;
};

//------------------------------------------------------------------------------
inline message_printer log_printer()
{
    return message_printer{&std::clog};
}

} // namespace ct
