#pragma once

#include <chrono>
#include <future>
#include <climits>
#include <cstddef>

namespace mcech::async_io
{
    class Future
    {
    public:
        Future() noexcept = default;
        Future(const Future&) = delete;
        Future(Future&& x) noexcept;
        ~Future();
        Future& operator=(const Future&) = delete;
        Future& operator=(Future&& x) noexcept;

        size_t get();
        bool valid() const noexcept;
        void wait() const;
        std::future_status wait_for(const std::chrono::milliseconds& rel_time) const;
        std::future_status wait_until(const std::chrono::system_clock::time_point& abs_time) const;

    private:
        friend class AsyncFileChannel;

        Future(intptr_t fd, void* job);

        intptr_t fd_ = -1;
        void* job_   = nullptr;
        mutable size_t result_  = 0;
        mutable uint32_t error_ = 0;
    };
}
