#include "ring.h"

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/ring_box.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

ring_status_t ring_init(ring_t *ring, const ring_config_t *config)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(ring, MEMKIT_RING_OBJ_BYTES);

    return memkit::c_api::ring_box::from(ring).init(config);
}

ring_status_t ring_create(
    ring_t **out_ring,
    size_t elem_size,
    size_t capacity,
    arena_t *arena,
    unsigned flags
)
{
    if (out_ring == NULL) {
        return RING_ERR_NULL;
    }
    if (elem_size == 0u || capacity == 0u) {
        return RING_ERR_INVALID;
    }

    ring_t *ring = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return RING_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &ring)) {
        return arena == NULL ? RING_ERR_OOM : RING_ERR_INVALID;
    }

    ring_config_t config = {
        .elem_size = elem_size,
        .capacity = capacity,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | RING_FLAG_OWNS_STORAGE,
            arena,
            {
                RING_FLAG_OWNS_STORAGE,
                RING_FLAG_DYNAMIC_STORAGE,
                RING_FLAG_ARENA_STORAGE,
                RING_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const ring_status_t status = ring_init(ring, &config);
    if (!ring_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, ring);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::ring_box::from(ring).set_c_flags(
            memkit::c_api::ring_box::from(ring).c_flags() | RING_FLAG_OWNS_SELF
        );
    }

    *out_ring = ring;
    return RING_OK;
}

void ring_deinit(ring_t *ring)
{
    if (ring == NULL) {
        return;
    }

    memkit::c_api::ring_box::from(ring).deinit();
    memkit::c_api::detail::zero_opaque_bytes(ring, MEMKIT_RING_OBJ_BYTES);
}

void ring_destroy(ring_t *ring)
{
    memkit::c_api::detail::destroy_owned_object<ring_t, memkit::c_api::ring_box>(
        ring,
        RING_FLAG_OWNS_SELF,
        RING_FLAG_DYNAMIC_STORAGE,
        ring_deinit
    );
}

size_t ring_size(const ring_t *ring)
{
    return ring != NULL ? memkit::c_api::ring_box::from(ring).core().size() : 0u;
}

size_t ring_capacity(const ring_t *ring)
{
    return ring != NULL ? memkit::c_api::ring_box::from(ring).core().capacity() : 0u;
}

bool ring_empty(const ring_t *ring)
{
    return ring == NULL || memkit::c_api::ring_box::from(ring).core().empty();
}

bool ring_full(const ring_t *ring)
{
    return ring != NULL && memkit::c_api::ring_box::from(ring).core().full();
}

void ring_clear(ring_t *ring)
{
    if (ring != NULL) {
        memkit::c_api::ring_box::from(ring).core().clear();
    }
}

ring_status_t ring_push_back(ring_t *ring, const void *elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }

    auto& core = memkit::c_api::ring_box::from(ring).core();
    const bool overwrite =
        memkit::detail::has(core.flags(), memkit::detail::ring_policy::overwrite_on_full);
    return static_cast<ring_status_t>(core.push_back(elem, overwrite));
}

ring_status_t ring_push_front(ring_t *ring, const void *elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }
    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().push_front(elem)
    );
}

ring_status_t ring_pop_front(ring_t *ring, void *out_elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }
    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().pop_front(out_elem)
    );
}

ring_status_t ring_pop_back(ring_t *ring, void *out_elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }
    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().pop_back(out_elem)
    );
}

ring_status_t ring_peek_front(const ring_t *ring, void *out_elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }
    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().peek_front(out_elem)
    );
}

ring_status_t ring_peek_back(const ring_t *ring, void *out_elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }
    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().peek_back(out_elem)
    );
}

ring_status_t ring_peek_at(const ring_t *ring, size_t index, void *out_elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }
    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().peek_at(index, out_elem)
    );
}

ring_status_t ring_set_at(ring_t *ring, size_t index, const void *elem)
{
    if (ring == NULL) {
        return RING_ERR_NULL;
    }
    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().set_at(index, elem)
    );
}

ring_status_t ring_foreach(const ring_t *ring, ring_visit_fn visit, void *user)
{
    if (ring == NULL || visit == NULL) {
        return RING_ERR_NULL;
    }

    return static_cast<ring_status_t>(
        memkit::c_api::ring_box::from(ring).core().foreach(
            [visit, user](const void* elem, size_t index) -> memkit::status {
                return static_cast<memkit::status>(visit(elem, index, user));
            }
        )
    );
}

size_t ring_readable_contiguous(const ring_t *ring, const void **out_ptr)
{
    if (ring == NULL) {
        return 0u;
    }
    return memkit::c_api::ring_box::from(ring).core().readable_contiguous(out_ptr);
}

size_t ring_writable_contiguous(ring_t *ring, void **out_ptr)
{
    if (ring == NULL) {
        return 0u;
    }
    return memkit::c_api::ring_box::from(ring).core().writable_contiguous(out_ptr);
}

void ring_commit_read(ring_t *ring, size_t elem_count)
{
    if (ring != NULL) {
        memkit::c_api::ring_box::from(ring).core().commit_read(elem_count);
    }
}

void ring_commit_write(ring_t *ring, size_t elem_count)
{
    if (ring != NULL) {
        memkit::c_api::ring_box::from(ring).core().commit_write(elem_count);
    }
}

} // extern "C"
