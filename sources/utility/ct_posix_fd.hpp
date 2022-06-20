#pragma once
#if !defined(_WIN32)
#include <unistd.h>

namespace ct {

struct posix_fd {
public:
    posix_fd() noexcept = default;
    ~posix_fd() noexcept;
    posix_fd(posix_fd &&other) noexcept;
    posix_fd &operator=(posix_fd &&other) noexcept;
    explicit operator bool() const noexcept;
    void reset() noexcept;
    static posix_fd owned(int fd) noexcept;
    int get() const noexcept;

private:
    posix_fd(const posix_fd &other) = delete;
    posix_fd &operator=(const posix_fd &other) = delete;

private:
    int m_fd = -1;
};

//------------------------------------------------------------------------------
inline posix_fd::~posix_fd() noexcept
{
    reset();
}

inline posix_fd::posix_fd(posix_fd &&other) noexcept
    : m_fd{other.m_fd}
{
    other.m_fd = -1;
}

inline posix_fd &posix_fd::operator=(posix_fd &&other) noexcept
{
    if (this != &other) {
        reset();
        m_fd = other.m_fd;
        other.m_fd = -1;
    }
    return *this;
}

inline posix_fd::operator bool() const noexcept
{
    return m_fd != -1;
}

inline void posix_fd::reset() noexcept
{
    if (m_fd != -1) {
        close(m_fd);
        m_fd = -1;
    }
}

inline posix_fd posix_fd::owned(int fd) noexcept
{
    posix_fd pfd;
    pfd.m_fd = fd;
    return pfd;
}

inline int posix_fd::get() const noexcept
{
    return m_fd;
}

} // namespace ct

#endif // !defined(_WIN32)
