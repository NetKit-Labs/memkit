#pragma once

#include "../status.hpp"

namespace memkit::c_api {

[[nodiscard]] inline status invoke_copy_fn(
    void* fn,
    void* dst,
    const void* src,
    void* user
) noexcept
{
    if (fn == nullptr) {
        return status::ok;
    }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

    using fn_t = int (*)(void*, const void*, void*);
    const status st = static_cast<status>(reinterpret_cast<fn_t>(fn)(dst, src, user));

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    return st;
}

inline void invoke_destroy_fn(void* fn, void* elem, void* user) noexcept
{
    if (fn == nullptr) {
        return;
    }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

    using fn_t = void (*)(void*, void*);
    reinterpret_cast<fn_t>(fn)(elem, user);

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

} // namespace memkit::c_api
