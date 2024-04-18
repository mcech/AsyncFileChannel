#pragma once

#include "Future.h"
#include "OpenOption.h"
#include <string>
#include <cstddef>
#include <cstdint>

namespace mcech::async_io
{
    class AsyncFileChannel
    {
    public:
        AsyncFileChannel() noexcept = default;
        explicit AsyncFileChannel(const std::string& path, OpenOption opt = OpenOption::READ | OpenOption::WRITE);
        AsyncFileChannel(const AsyncFileChannel&) = delete;
        AsyncFileChannel(AsyncFileChannel&& x) noexcept;
        ~AsyncFileChannel();
        AsyncFileChannel& operator=(const AsyncFileChannel&) = delete;
        AsyncFileChannel& operator=(AsyncFileChannel&& x) noexcept;

        void open(const std::string& path, OpenOption opt = OpenOption::READ | OpenOption::WRITE);
        bool is_open() const noexcept;
        uint64_t size() const;
        void resize(uint64_t size);
        size_t block_size() const;
        Future read(uint64_t off, void* buf, size_t len);
        Future write(uint64_t off, const void* buf, size_t len);
        void sync(bool meta);
        void close() noexcept;

    private:
        intptr_t fd_ = -1;
    };
}
