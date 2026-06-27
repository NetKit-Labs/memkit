#pragma once

#include "create_object.hpp"

#include "../../memkit_config.h"

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

namespace memkit::c_api::detail {

template<typename ObjectT>
inline void zero_opaque_bytes(ObjectT* object, std::size_t object_bytes) noexcept
{
    for (std::size_t i = 0u; i < object_bytes; ++i) {
        object->bytes[i] = 0u;
    }
}

struct owned_storage_flags {
    unsigned owns_storage;
    unsigned dynamic_storage;
    unsigned arena_storage;
    unsigned owns_self;
};

[[nodiscard]] inline unsigned owned_create_config_flags(
    unsigned user_flags,
    arena_t* arena,
    const owned_storage_flags& flag,
    unsigned extra_flags = 0u
) noexcept
{
    unsigned flags = user_flags | flag.owns_storage | extra_flags;

#if MEMKIT_ALLOW_HEAP
    if (arena == nullptr) {
        flags |= flag.dynamic_storage | flag.owns_self;
    } else {
        flags |= flag.arena_storage;
    }
#else
    (void)arena;
    flags |= flag.arena_storage;
#endif

    return flags;
}

template<typename ObjectT, typename Box, typename DeinitFn>
inline void destroy_owned_object(
    ObjectT* object,
    unsigned flag_owns_self,
    unsigned flag_dynamic_storage,
    DeinitFn deinit_fn
) noexcept
{
    if (object == nullptr) {
        return;
    }

    const unsigned saved_flags = Box::from(object).c_flags();
    deinit_fn(object);

#if MEMKIT_ALLOW_HEAP
    if ((saved_flags & flag_owns_self) != 0u &&
        (saved_flags & flag_dynamic_storage) != 0u) {
        std::free(object);
    }
#else
    (void)saved_flags;
    (void)flag_owns_self;
    (void)flag_dynamic_storage;
#endif
}

} // namespace memkit::c_api::detail
