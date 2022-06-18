#pragma once
#if !defined(_WIN32)
#include <unistd.h>
#include <fcntl.h>
#include <system_error>

namespace ct {

struct posix_pipe {
public:
    posix_pipe() = default;
    ~posix_pipe() noexcept;
    posix_pipe(posix_pipe &&other) noexcept;
    posix_pipe &operator=(posix_pipe &&other) noexcept;
    explicit operator bool() const noexcept { return m_reader_fd != -1; }
    void reset() noexcept;
    static posix_pipe create();
    static posix_pipe create(std::error_code &ec) noexcept;
    int reader_fd() const noexcept { return m_reader_fd; }
    int writer_fd() const noexcept { return m_writer_fd; }

private:
    int m_reader_fd = -1;
    int m_writer_fd = -1;
};

} // namespace ct

#endif // !defined(_WIN32)
