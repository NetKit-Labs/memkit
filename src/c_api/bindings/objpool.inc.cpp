#include "objpool.h"

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/objpool_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/objpool_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

size_t objpool_used_bits_bytes(size_t capacity)
{
    return memkit::detail::objpool_core<memkit::detail::runtime_element_policy>::used_bits_bytes(
        capacity
    );
}

size_t objpool_free_stack_bytes(size_t capacity)
{
    return memkit::detail::objpool_core<memkit::detail::runtime_element_policy>::free_stack_bytes(
        capacity
    );
}

size_t objpool_storage_bytes(size_t elem_size, size_t capacity)
{
    return elem_size * capacity;
}

objpool_status_t objpool_init(objpool_t *pool, const objpool_config_t *config)
{
    if (pool == NULL) {
        return OBJPOOL_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(pool, MEMKIT_OBJPOOL_OBJ_BYTES);

    return memkit::c_api::objpool_box::from(pool).init(config);
}

objpool_status_t objpool_create(
    objpool_t **out_pool,
    size_t elem_size,
    size_t capacity,
    arena_t *arena,
    unsigned flags
)
{
    if (out_pool == NULL) {
        return OBJPOOL_ERR_NULL;
    }
    if (elem_size == 0u || capacity == 0u) {
        return OBJPOOL_ERR_INVALID;
    }

    objpool_t *pool = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return OBJPOOL_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &pool)) {
        return arena == NULL ? OBJPOOL_ERR_OOM : OBJPOOL_ERR_INVALID;
    }

    objpool_config_t config = {
        .elem_size = elem_size,
        .capacity = capacity,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | OBJPOOL_FLAG_OWNS_STORAGE,
            arena,
            {
                OBJPOOL_FLAG_OWNS_STORAGE,
                OBJPOOL_FLAG_DYNAMIC_STORAGE,
                OBJPOOL_FLAG_ARENA_STORAGE,
                OBJPOOL_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const objpool_status_t status = objpool_init(pool, &config);
    if (!objpool_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, pool);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::objpool_box::from(pool).set_c_flags(
            memkit::c_api::objpool_box::from(pool).c_flags() | OBJPOOL_FLAG_OWNS_SELF
        );
    }

    *out_pool = pool;
    return OBJPOOL_OK;
}

void objpool_deinit(objpool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    memkit::c_api::objpool_box::from(pool).deinit();
    memkit::c_api::detail::zero_opaque_bytes(pool, MEMKIT_OBJPOOL_OBJ_BYTES);
}

void objpool_destroy(objpool_t *pool)
{
    memkit::c_api::detail::destroy_owned_object<objpool_t, memkit::c_api::objpool_box>(
        pool,
        OBJPOOL_FLAG_OWNS_SELF,
        OBJPOOL_FLAG_DYNAMIC_STORAGE,
        objpool_deinit
    );
}

size_t objpool_size(const objpool_t *pool)
{
    return pool != NULL ? memkit::c_api::objpool_box::from(pool).core().size() : 0u;
}

size_t objpool_capacity(const objpool_t *pool)
{
    return pool != NULL ? memkit::c_api::objpool_box::from(pool).core().capacity() : 0u;
}

size_t objpool_available(const objpool_t *pool)
{
    return pool != NULL ? memkit::c_api::objpool_box::from(pool).core().available() : 0u;
}

bool objpool_empty(const objpool_t *pool)
{
    return pool == NULL || memkit::c_api::objpool_box::from(pool).core().empty();
}

bool objpool_full(const objpool_t *pool)
{
    return pool == NULL || memkit::c_api::objpool_box::from(pool).core().full();
}

void objpool_clear(objpool_t *pool)
{
    if (pool != NULL) {
        memkit::c_api::objpool_box::from(pool).core().clear();
    }
}

objpool_status_t objpool_alloc(objpool_t *pool, void **out_elem)
{
    if (pool == NULL || out_elem == NULL) {
        return OBJPOOL_ERR_NULL;
    }
    return memkit::c_api::to_objpool_status(
        memkit::c_api::objpool_box::from(pool).core().alloc(out_elem)
    );
}

objpool_status_t objpool_alloc_copy(objpool_t *pool, const void *src, void **out_elem)
{
    if (pool == NULL || src == NULL || out_elem == NULL) {
        return OBJPOOL_ERR_NULL;
    }
    return memkit::c_api::to_objpool_status(
        memkit::c_api::objpool_box::from(pool).core().alloc_copy(src, out_elem)
    );
}

objpool_status_t objpool_index(const objpool_t *pool, const void *elem, size_t *out_index)
{
    if (pool == NULL || elem == NULL || out_index == NULL) {
        return OBJPOOL_ERR_NULL;
    }

    std::size_t index = 0u;
    const memkit::status st =
        memkit::c_api::objpool_box::from(pool).core().index(elem, index);
    if (st == memkit::status::ok) {
        *out_index = index;
    }
    return memkit::c_api::to_objpool_status(st);
}

bool objpool_contains(const objpool_t *pool, const void *elem)
{
    return pool != NULL && memkit::c_api::objpool_box::from(pool).core().contains(elem);
}

objpool_status_t objpool_free(objpool_t *pool, void *elem)
{
    if (pool == NULL) {
        return OBJPOOL_ERR_NULL;
    }
    return memkit::c_api::to_objpool_status(
        memkit::c_api::objpool_box::from(pool).core().free(elem)
    );
}

objpool_status_t objpool_foreach(const objpool_t *pool, objpool_visit_fn visit, void *user)
{
    if (pool == NULL || visit == NULL) {
        return OBJPOOL_ERR_NULL;
    }

    std::size_t visit_index = 0u;
    const memkit::status st = memkit::c_api::objpool_box::from(pool).core().for_each(
        [visit, user, &visit_index](const void *elem, std::size_t) -> memkit::status {
            const objpool_status_t status = visit(elem, visit_index, user);
            ++visit_index;
            return objpool_status_ok(status) ? memkit::status::ok : memkit::status::invalid;
        }
    );

    if (st != memkit::status::ok) {
        return OBJPOOL_ERR_INVALID;
    }
    return OBJPOOL_OK;
}

} // extern "C"
