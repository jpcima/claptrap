#pragma once
#if !defined(_WIN32)
#include "ct_posix_fd.hpp"
#include <system_error>

namespace ct {

struct posix_pipe {
public:
    posix_pipe() noexcept = default;
    ~posix_pipe() noexcept = default;
    posix_pipe(posix_pipe &&other) noexcept = default;
    posix_pipe &operator=(posix_pipe &&other) noexcept = default;
    explicit operator bool() const noexcept;
    void reset() noexcept;
    static posix_pipe create();
    static posix_pipe create(std::error_code &ec) noexcept;
    int reader_fd() const noexcept;
    int writer_fd() const noexcept;

private:
    posix_pipe(const posix_pipe &other) = delete;
    posix_pipe &operator=(const posix_pipe &other) = delete;

private:
    posix_fd m_reader_fd;
    posix_fd m_writer_fd;
};

//------------------------------------------------------------------------------
inline posix_pipe::operator bool() const noexcept
{
    return bool{m_reader_fd};
}

inline void posix_pipe::reset() noexcept
{
    m_reader_fd.reset();
    m_writer_fd.reset();
}

inline int posix_pipe::reader_fd() const noexcept
{
    return m_reader_fd.get();
}

inline int posix_pipe::writer_fd() const noexcept
{
    return m_writer_fd.get();
}

} // namespace ct

#endif // !defined(_WIN32)
