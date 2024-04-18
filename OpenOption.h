#pragma once

#include <cstdint>

namespace mcech::async_io
{
    enum class OpenOption : uint8_t
    {
        READ       = 0b00000000,  // Open for read access.
        WRITE      = 0b00000001,  // Open for write access.
        APPEND     = 0b00000010,  // Append all writes.
        CREATE     = 0b00000100,  // Create new file if it does not exist. Ignored if CREATE_NEW option is set.
        CREATE_NEW = 0b00001000,  // Create new file, failing if already exists.
        TRUNCATE   = 0b00010000,  // Truncate if file exists and is opened for WRITE.
        SYNC       = 0b00100000,  // Every update to content or metadata is written immediately.
        DSYNC      = 0b01000000,  // Every update to content is written immediately. Ignored if SYNC option is set.
        DIRECT     = 0b10000000   // Direct I/O is used.
    };

    constexpr OpenOption operator|(OpenOption lhs, OpenOption rhs)
    {
        return static_cast<OpenOption>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }
}
