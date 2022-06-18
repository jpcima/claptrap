#include "ct_posix_pipe.hpp"
#if !defined(_WIN32)
#include <cerrno>

namespace ct {

posix_pipe::~posix_pipe() noexcept
{
    reset();
}

posix_pipe::posix_pipe(posix_pipe &&other) noexcept
{
    m_reader_fd = other.m_reader_fd;
    m_writer_fd = other.m_writer_fd;
    other.m_reader_fd = -1;
    other.m_writer_fd = -1;
}

posix_pipe &posix_pipe::operator=(posix_pipe &&other) noexcept
{
    if (this != &other) {
        reset();
        m_reader_fd = other.m_reader_fd;
        m_writer_fd = other.m_writer_fd;
        other.m_reader_fd = -1;
        other.m_writer_fd = -1;
    }
    return *this;
}

void posix_pipe::reset() noexcept
{
    if (*this) {
        close(m_reader_fd);
        close(m_writer_fd);
        m_reader_fd = -1;
        m_writer_fd = -1;
    }
}

posix_pipe posix_pipe::create()
{
    std::error_code ec;
    posix_pipe pp = create(ec);
    if (ec)
        throw std::system_error{ec};

    return pp;
}

posix_pipe posix_pipe::create(std::error_code &ec) noexcept
{
    if (ec)
        return {};

    int pipedes[2];
#if defined(__APPLE__)
    if (pipe(pipedes) == -1)
#else
    if (pipe2(pipedes, O_CLOEXEC) == -1)
#endif
    {
        ec = std::error_code{errno, std::generic_category()};
        return {};
    }

#if defined(__APPLE__)
    fcntl(pipedes[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipedes[1], F_SETFD, FD_CLOEXEC);
#endif

    posix_pipe pp;
    pp.m_reader_fd = pipedes[0];
    pp.m_writer_fd = pipedes[1];

    return pp;
}

} // namespace ct

#endif // !defined(_WIN32)
