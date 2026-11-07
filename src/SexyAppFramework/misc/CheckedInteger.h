// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Sexy
{
// Legacy drawing, resource and on-disk interfaces use 32-bit integers.
// Reject out-of-range input in release builds rather than silently truncating it.
template<class To, class From>
constexpr To CheckedInteger(From value)
{
    static_assert(std::is_integral_v<To> && std::is_integral_v<From>);
    if (!std::in_range<To>(value))
        throw std::overflow_error("Integer exceeds the legacy interface range");
    return static_cast<To>(value);
}

// Preserve the legacy -1 sentinel without narrowing size_t::max to signed int.
inline int LegacyStringIndex(std::size_t position)
{
    return position == std::string_view::npos ? -1 : CheckedInteger<int>(position);
}
}
