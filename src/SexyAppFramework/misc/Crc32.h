// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>
#include <zlib.h>
namespace Sexy {
inline uint32_t Crc32(uint32_t seed, const Bytef* data, size_t length)
{
    // zlib CRC-32 always returns a 32-bit checksum (zero-extended uLong).
    return static_cast<uint32_t>(crc32_z(seed, data, length));
}
}
