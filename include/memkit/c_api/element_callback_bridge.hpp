#pragma once

#include "../detail/element_policy.hpp"
#include "callback_bridge.hpp"

#include <cstddef>
#include <cstring>

namespace memkit::c_api {

struct element_callback_bridge {
    void*       copy_opaque    = nullptr;
    void*       destroy_opaque = nullptr;
    void*       user           = nullptr;
    std::size_t elem_size      = 0u;

    void set(std::size_t size, void* copy, void* destroy, void* user_ptr) noexcept
    {
        elem_size      = size;
        copy_opaque    = copy;
        destroy_opaque = destroy;
        user           = user_ptr;
    }

    [[nodiscard]] static status copy_trampoline(void* dst, const void* src, void* ctx) noexcept
    {
        auto* bridge = static_cast<element_callback_bridge*>(ctx);
        if (bridge->copy_opaque != nullptr) {
            return invoke_copy_fn(bridge->copy_opaque, dst, src, bridge->user);
        }
        if (bridge->elem_size > 0u) {
            std::memcpy(dst, src, bridge->elem_size);
        }
        return status::ok;
    }

    static void destroy_trampoline(void* elem, void* ctx) noexcept
    {
        auto* bridge = static_cast<element_callback_bridge*>(ctx);
        if (bridge->destroy_opaque != nullptr) {
            invoke_destroy_fn(bridge->destroy_opaque, elem, bridge->user);
        }
    }

    [[nodiscard]] ::memkit::detail::runtime_element_policy as_policy() const noexcept
    {
        return ::memkit::detail::runtime_element_policy{
            elem_size,
            copy_trampoline,
            destroy_opaque != nullptr ? destroy_trampoline : nullptr,
            const_cast<element_callback_bridge*>(this),
        };
    }
};

} // namespace memkit::c_api
