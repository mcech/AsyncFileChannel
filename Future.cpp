#include "Future.h"

#include <limits>
#include <utility>
#include <cerrno>
#ifdef _WIN32
#include <Windows.h>
#else
#include <aio.h>
#endif

namespace mcech::async_io
{
    Future::Future(Future&& x) noexcept
    {
        fd_     = std::exchange(x.fd_, -1);
        job_    = std::exchange(x.job_, nullptr);
        result_ = std::exchange(x.result_, 0);
        error_  = std::exchange(x.error_, 0);
    }

    Future::~Future()
    {
        if (valid())
        {
            get();
        }
    }

    Future& Future::operator=(Future&& x) noexcept
    {
        if (std::addressof(x) != this)
        {
            if (valid())
            {
                try {
                    get();
                }
                catch (...) {
                    // ignore
                }
            }
            fd_     = std::exchange(x.fd_, -1);
            job_    = std::exchange(x.job_, nullptr);
            result_ = std::exchange(x.result_, 0);
            error_  = std::exchange(x.error_, 0);
        }
        return *this;
    }

    size_t Future::get()
    {
        wait();
        fd_ = -1;
#ifdef _WIN32
        delete static_cast<OVERLAPPED*>(job_); job_ = nullptr;
#else
        delete static_cast<aiocb64*>(job_); job_ = nullptr;
#endif
        if (result_ == 0)
        {
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(error_)));
        }
        else
        {
            return result_;
        }
    }

    bool Future::valid() const noexcept
    {
        return job_ != nullptr;
    }

    void Future::wait() const
    {
        if (!valid())
        {
            throw std::future_error(std::future_errc::no_state);
        }

#ifdef _WIN32
        DWORD number_of_bytes_transferred;
        if (GetOverlappedResult((HANDLE)fd_, static_cast<OVERLAPPED*>(job_), &number_of_bytes_transferred, TRUE) == FALSE)
        {
            error_ = GetLastError();
        }
        else
        {
            result_ = number_of_bytes_transferred;
        }
#else
        aiocb64* aiocbs[] = {static_cast<aiocb64*>(job_)};
        if (aio_suspend64(aiocbs, 1, nullptr) != 0)
        {
            error_ = errno;
        }
        else
        {
            ssize_t number_of_bytes_transferred = aio_return64(static_cast<aiocb64*>(job_));
            result_ = number_of_bytes_transferred;
        }
#endif
    }

    std::future_status Future::wait_for(const std::chrono::milliseconds& rel_time) const
    {
        if (!valid())
        {
            throw std::future_error(std::future_errc::no_state);
        }

#ifdef _WIN32
        DWORD number_of_bytes_transferred;
        DWORD timeout = static_cast<DWORD>(rel_time.count() < (std::numeric_limits<DWORD>::max)() ? rel_time.count() : (std::numeric_limits<DWORD>::max)());
        if (GetOverlappedResultEx((HANDLE)fd_, static_cast<OVERLAPPED*>(job_), &number_of_bytes_transferred, timeout, FALSE) == FALSE)
        {
            error_ = GetLastError();
            if (error_ == ERROR_IO_INCOMPLETE || error_ == WAIT_TIMEOUT)
            {
                return std::future_status::timeout;
            }
            else
            {
                throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(error_)));
            }
        }
        else
        {
            result_ = number_of_bytes_transferred;
            return std::future_status::ready;
        }
#else
        aiocb64* aiocbs[] = {static_cast<aiocb64*>(job_)};
        timespec timeout;
        timeout.tv_sec = rel_time.count() / 1000;
        timeout.tv_nsec = (rel_time.count() % 1000) * 1000000;
        if (aio_suspend64(aiocbs, 1, &timeout) != 0)
        {
            error_ = errno;
            if (error_ == EAGAIN)
            {
                return std::future_status::timeout;
            }
            else
            {
                throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(error_)));
            }
        }
        else
        {
            ssize_t number_of_bytes_transferred = aio_return64(static_cast<aiocb64*>(job_));
            result_ = number_of_bytes_transferred;
            return std::future_status::ready;
        }
#endif
    }

    std::future_status Future::wait_until(const std::chrono::system_clock::time_point& abs_time) const
    {
        return wait_for(std::chrono::duration_cast<std::chrono::milliseconds>(abs_time - std::chrono::system_clock::now()));
    }

    Future::Future(intptr_t fd_, void* job_) : fd_(fd_), job_(job_)
    {
    }
}
