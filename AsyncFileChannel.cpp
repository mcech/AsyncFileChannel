#include "AsyncFileChannel.h"

#include <ios>
#include <utility>
#include <cstring>
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#include <aio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace mcech::async_io
{
    constexpr uint8_t operator&(OpenOption lhs, OpenOption rhs)
    {
        return static_cast<int>(lhs) & static_cast<int>(rhs);
    }

    AsyncFileChannel::AsyncFileChannel(const std::string& path, OpenOption opt)
    {
        open(path, opt);
    }

    AsyncFileChannel::AsyncFileChannel(AsyncFileChannel&& x) noexcept
    {
        fd_ = std::exchange(x.fd_, -1);
    }

    AsyncFileChannel::~AsyncFileChannel()
    {
        if (is_open())
        {
            close();
        }
    }

    AsyncFileChannel& AsyncFileChannel::operator=(AsyncFileChannel&& x) noexcept
    {
        if (std::addressof(x) != this)
        {
            if (is_open())
            {
                close();
            }
            fd_ = std::exchange(x.fd_, -1);
        }
        return *this;
    }

    void AsyncFileChannel::open(const std::string& path, OpenOption opt)
    {
        if (is_open())
        {
            close();
        }

    #ifdef _WIN32
        DWORD desired_access = FILE_GENERIC_READ;
        if (opt & OpenOption::WRITE)
        {
            desired_access |= FILE_GENERIC_WRITE;
            if (opt & OpenOption::APPEND)
            {
                desired_access |= FILE_APPEND_DATA;
            }
        }

        DWORD creation_disposition;
    #pragma push_macro("CREATE_NEW")
    #undef CREATE_NEW
        if (opt & OpenOption::CREATE_NEW)
    #pragma pop_macro("CREATE_NEW")
        {
            creation_disposition = CREATE_NEW;
        }
        else if ((opt & OpenOption::CREATE) && (opt & OpenOption::TRUNCATE) && (opt & OpenOption::WRITE))
        {
            creation_disposition = CREATE_ALWAYS;
        }
        else if (opt & OpenOption::CREATE)
        {
            creation_disposition = OPEN_ALWAYS;
        }
        else if ((opt & OpenOption::TRUNCATE) && (opt & OpenOption::WRITE))
        {
            creation_disposition = TRUNCATE_EXISTING;
        }
        else
        {
            creation_disposition = OPEN_EXISTING;
        }

        DWORD flags = FILE_FLAG_OVERLAPPED;
        if ((opt & OpenOption::SYNC) || (opt & OpenOption::DSYNC))
        {
            flags |= FILE_FLAG_WRITE_THROUGH;
        }
        if (opt & OpenOption::DIRECT)
        {
            flags |= FILE_FLAG_NO_BUFFERING;
        }

        fd_ = (intptr_t)CreateFileA(path.c_str(),
                                    desired_access,
                                    NULL,
                                    NULL,
                                    creation_disposition,
                                    flags,
                                    NULL);
        if (fd_ == -1)
        {
            DWORD err = GetLastError();
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
    #else
        int flags = O_RDONLY;
        if (opt & OpenOption::WRITE)
        {
            flags = O_RDWR;
            if (opt & OpenOption::APPEND)
            {
                flags |= O_APPEND;
            }
        }

        if (opt & OpenOption::CREATE_NEW)
        {
            flags |= (O_CREAT | O_EXCL);
        }
        else if ((opt & OpenOption::CREATE) && (opt & OpenOption::TRUNCATE) && (opt & OpenOption::WRITE))
        {
            flags |= (O_CREAT | O_TRUNC);
        }
        else if (opt & OpenOption::CREATE)
        {
            flags |= O_CREAT;
        }
        else if ((opt & OpenOption::TRUNCATE) && (opt & OpenOption::WRITE))
        {
            flags |= O_TRUNC;
        }
        else
        {
            ;  // do nothing
        }

        if (opt & OpenOption::SYNC)
        {
            flags |= O_SYNC;
        }
        else if (opt & OpenOption::DSYNC)
        {
            flags |= O_DSYNC;
        }
        if (opt & OpenOption::DIRECT)
        {
            flags |= O_DIRECT;
        }

        fd_ = open64(path.c_str(), flags);
        if (fd_ == -1)
        {
            int err = errno;
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
    #endif
    }

    bool AsyncFileChannel::is_open() const noexcept
    {
        return fd_ != -1;
    }

    uint64_t AsyncFileChannel::size() const
    {
    #ifdef _WIN32
        FILE_STANDARD_INFO file_info = {};
        if (GetFileInformationByHandleEx((HANDLE)fd_, FileStandardInfo, &file_info, sizeof(file_info)) == FALSE)
        {
            DWORD err = GetLastError();
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
        return file_info.EndOfFile.QuadPart;
    #else
        struct stat64 stat = {};
        if (fstat64(fd_, &stat) == -1)
        {
            int err = errno;
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
        return stat.st_size;
    #endif
    }

    void AsyncFileChannel::resize(uint64_t len)
    {
    #ifdef _WIN32
        FILE_END_OF_FILE_INFO eof_info; eof_info.EndOfFile.QuadPart = len;
        if (SetFileInformationByHandle((HANDLE)fd_, FileEndOfFileInfo, &eof_info, sizeof(eof_info)) == FALSE)
        {
            DWORD err = GetLastError();
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
    #else
        if (posix_fallocate64(fd_, 0, len) != 0)
        {
            int err = errno;
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
    #endif
    }

    size_t AsyncFileChannel::block_size() const
    {
    #ifdef _WIN32
        FILE_STORAGE_INFO  file_storage_info = {};
        if (GetFileInformationByHandleEx((HANDLE)fd_, FileStorageInfo,  &file_storage_info,  sizeof(file_storage_info))  == FALSE)
        {
            DWORD err = GetLastError();
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
        return file_storage_info.PhysicalBytesPerSectorForPerformance;
    #else
        struct stat64 stat = {};
        if (fstat64(fd_, &stat) == -1)
        {
            int err = errno;
            throw std::ios_base::failure(__func__, std::make_error_code(static_cast<std::errc>(err)));
        }
        return stat.st_blksize;
    #endif
    }

    Future AsyncFileChannel::read(uint64_t off, void* buf, size_t len)
    {
    #ifdef _WIN32
        OVERLAPPED* overlapped = new OVERLAPPED{};
        overlapped->OffsetHigh = off >> 32;
        overlapped->Offset = static_cast<DWORD>(off);
        DWORD dummy;
        ReadFile((HANDLE)fd_, buf, static_cast<DWORD>(len), &dummy, overlapped);
        return Future(fd_, overlapped);
    #else
        aiocb64* aiocb = new aiocb64{};
        aiocb->aio_fildes = fd_;
        aiocb->aio_offset = off;
        aiocb->aio_buf = buf;
        aiocb->aio_nbytes = len;
        aiocb->aio_sigevent.sigev_notify = SIGEV_NONE;
        aio_read64(aiocb);
        return Future(fd_, aiocb);
    #endif
    }

    Future AsyncFileChannel::write(uint64_t off, const void* buf, size_t len)
    {
    #ifdef _WIN32
        OVERLAPPED* overlapped = new OVERLAPPED{};
        overlapped->OffsetHigh = off >> 32;
        overlapped->Offset = static_cast<DWORD>(off);
        DWORD dummy;
        WriteFile((HANDLE)fd_, buf, static_cast<DWORD>(len), &dummy, overlapped);
        return Future(fd_, overlapped);
    #else
        aiocb64* aiocb = new aiocb64{};
        aiocb->aio_fildes = fd_;
        aiocb->aio_offset = off;
        aiocb->aio_buf = const_cast<void*>(buf);
        aiocb->aio_nbytes = len;
        aiocb->aio_sigevent.sigev_notify = SIGEV_NONE;
        aio_write64(aiocb);
        return Future(fd_, aiocb);
    #endif
    }

    void AsyncFileChannel::sync([[maybe_unused]] bool meta)
    {
    #ifdef _WIN32
        FlushFileBuffers((HANDLE)fd_);
    #else
        if (meta)
        {
            fsync(fd_);
        }
        else
        {
            fdatasync(fd_);
        }
    #endif
    }

    void AsyncFileChannel::close() noexcept
    {
        sync(true);
    #ifdef _WIN32
        CloseHandle((HANDLE)fd_);
    #else
        ::close(fd_);
    #endif
        fd_ = -1;
    }
}
