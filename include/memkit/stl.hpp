#pragma once

/*
 * Standard library surface exposed by memkit.
 *
 * MCU  — zero-cost abstractions only (array, span, optional, less, …).
 *        std::vector / std::string are NOT provided; use them on your own if needed.
 * MPU  — same zero-cost types, plus optional heap STL aliases when
 *        MEMKIT_ALLOW_HEAP_STL is defined (MEMKIT_USE_STL=1 on MPU builds).
 */

#include "config.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#if MEMKIT_ALLOW_HEAP_STL
#include <string>
#include <vector>
#endif

namespace memkit::stl {

/* --- Fixed-size storage (stack/static, zero overhead vs C array) ------------ */

template<typename T, std::size_t N>
using array = std::array<T, N>;

/* --- Non-owning views (pointer + length, no allocation) --------------------- */

template<typename T>
using span = std::span<T>;

using byte_span       = std::span<std::byte>;
using const_byte_span = std::span<const std::byte>;

using string_view = std::string_view;

/* --- Optional values (empty state optimized for most T) ------------------- */

template<typename T>
using optional = std::optional<T>;

inline constexpr auto nullopt = std::nullopt;

/* --- Comparisons and hashing (inline, zero overhead when specialized) ----- */

using std::equal_to;
using std::greater;
using std::hash;
using std::less;
using std::not_equal_to;

/* --- Small utilities ------------------------------------------------------ */

using std::clamp;
using std::exchange;
using std::forward;
using std::max;
using std::min;
using std::move;
using std::swap;

#if MEMKIT_ALLOW_HEAP_STL
/* MPU-only optional aliases; not available on MCU builds. */
template<typename T>
using vector = std::vector<T>;

using string      = std::string;
using string_view = std::string_view;
#endif

} // namespace memkit::stl

#if MEMKIT_TARGET_MCU
static_assert(!MEMKIT_ALLOW_HEAP_STL, "MCU builds must not expose heap STL via memkit");
#endif
