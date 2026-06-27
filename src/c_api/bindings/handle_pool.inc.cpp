#include "handle_pool.h"

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/handle_pool_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/handle_pool_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

size_t handle_pool_generations_bytes(size_t capacity)
{
    return memkit::detail::handle_pool_core::generations_bytes(capacity);
}

size_t handle_pool_free_stack_bytes(size_t capacity)
{
    return memkit::detail::handle_pool_core::free_stack_bytes(capacity);
}

size_t handle_pool_storage_bytes(size_t elem_size, size_t capacity)
{
    return elem_size * capacity;
}

handle_pool_status_t handle_pool_init(handle_pool_t* pool, const handle_pool_config_t* config)
{
    if (pool == NULL) {
        return HANDLE_POOL_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(pool, MEMKIT_HANDLE_POOL_OBJ_BYTES);

    return memkit::c_api::handle_pool_box::from(pool).init(config);
}

handle_pool_status_t handle_pool_create(
    handle_pool_t** out_pool,
    size_t elem_size,
    size_t capacity,
    arena_t* arena,
    unsigned flags
)
{
    if (out_pool == NULL) {
        return HANDLE_POOL_ERR_NULL;
    }
    if (elem_size == 0u || capacity == 0u) {
        return HANDLE_POOL_ERR_INVALID;
    }

    handle_pool_t* pool = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return HANDLE_POOL_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &pool)) {
        return arena == NULL ? HANDLE_POOL_ERR_OOM : HANDLE_POOL_ERR_INVALID;
    }

    handle_pool_config_t config = {
        .elem_size = elem_size,
        .capacity  = capacity,
        .arena     = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | HANDLE_POOL_FLAG_OWNS_STORAGE | HANDLE_POOL_FLAG_FIXED_CAPACITY,
            arena,
            {
                HANDLE_POOL_FLAG_OWNS_STORAGE,
                HANDLE_POOL_FLAG_DYNAMIC_STORAGE,
                HANDLE_POOL_FLAG_ARENA_STORAGE,
                HANDLE_POOL_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const handle_pool_status_t status = handle_pool_init(pool, &config);
    if (!handle_pool_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, pool);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::handle_pool_box::from(pool).set_c_flags(
            memkit::c_api::handle_pool_box::from(pool).c_flags() | HANDLE_POOL_FLAG_OWNS_SELF
        );
    }

    *out_pool = pool;
    return HANDLE_POOL_OK;
}

void handle_pool_deinit(handle_pool_t* pool)
{
    if (pool == NULL) {
        return;
    }
    memkit::c_api::handle_pool_box::from(pool).deinit();
}

void handle_pool_destroy(handle_pool_t* pool)
{
    memkit::c_api::detail::destroy_owned_object<handle_pool_t, memkit::c_api::handle_pool_box>(
        pool,
        HANDLE_POOL_FLAG_OWNS_SELF,
        HANDLE_POOL_FLAG_DYNAMIC_STORAGE,
        handle_pool_deinit
    );
}

size_t handle_pool_size(const handle_pool_t* pool)
{
    if (pool == NULL) {
        return 0u;
    }
    return memkit::c_api::handle_pool_box::from(pool).core().size();
}

size_t handle_pool_capacity(const handle_pool_t* pool)
{
    if (pool == NULL) {
        return 0u;
    }
    return memkit::c_api::handle_pool_box::from(pool).core().capacity();
}

size_t handle_pool_available(const handle_pool_t* pool)
{
    if (pool == NULL) {
        return 0u;
    }
    return memkit::c_api::handle_pool_box::from(pool).core().available();
}

bool handle_pool_empty(const handle_pool_t* pool)
{
    if (pool == NULL) {
        return true;
    }
    return memkit::c_api::handle_pool_box::from(pool).core().empty();
}

bool handle_pool_full(const handle_pool_t* pool)
{
    if (pool == NULL) {
        return false;
    }
    return memkit::c_api::handle_pool_box::from(pool).core().full();
}

handle_pool_status_t handle_pool_acquire(handle_pool_t* pool, void** out_elem, handle_t* out_handle)
{
    if (pool == NULL || out_elem == NULL || out_handle == NULL) {
        return HANDLE_POOL_ERR_NULL;
    }

    return memkit::c_api::to_handle_pool_status(
        memkit::c_api::handle_pool_box::from(pool).core().acquire(out_elem, out_handle)
    );
}

handle_pool_status_t handle_pool_release(handle_pool_t* pool, handle_t handle)
{
    if (pool == NULL) {
        return HANDLE_POOL_ERR_NULL;
    }

    return memkit::c_api::to_handle_pool_status(
        memkit::c_api::handle_pool_box::from(pool).core().release(handle)
    );
}

handle_pool_status_t handle_pool_get(const handle_pool_t* pool, handle_t handle, void** out_elem)
{
    if (pool == NULL || out_elem == NULL) {
        return HANDLE_POOL_ERR_NULL;
    }

    return memkit::c_api::to_handle_pool_status(
        memkit::c_api::handle_pool_box::from(pool).core().get(handle, out_elem)
    );
}

bool handle_pool_valid(const handle_pool_t* pool, handle_t handle)
{
    if (pool == NULL) {
        return false;
    }
    return memkit::c_api::handle_pool_box::from(pool).core().valid(handle);
}

} // extern "C"
