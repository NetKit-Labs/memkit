#include "queue.h"

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/queue_box.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

queue_status_t queue_init(queue_t* queue, const queue_config_t* config)
{
    if (queue == NULL) {
        return QUEUE_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(queue, MEMKIT_QUEUE_OBJ_BYTES);

    return memkit::c_api::queue_box::from(queue).init(config);
}

queue_status_t queue_create(
    queue_t** out_queue,
    size_t elem_size,
    size_t initial_capacity,
    arena_t* arena,
    unsigned flags
)
{
    if (out_queue == NULL) {
        return QUEUE_ERR_NULL;
    }
    if (elem_size == 0u) {
        return QUEUE_ERR_INVALID;
    }

    if (initial_capacity == 0u) {
        initial_capacity = 1u;
    }

    queue_t* queue = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return QUEUE_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &queue)) {
        return arena == NULL ? QUEUE_ERR_OOM : QUEUE_ERR_INVALID;
    }

    queue_config_t config = {
        .elem_size = elem_size,
        .capacity = initial_capacity,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | QUEUE_FLAG_OWNS_STORAGE | QUEUE_FLAG_GROWABLE,
            arena,
            {
                QUEUE_FLAG_OWNS_STORAGE,
                QUEUE_FLAG_DYNAMIC_STORAGE,
                QUEUE_FLAG_ARENA_STORAGE,
                QUEUE_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const queue_status_t status = queue_init(queue, &config);
    if (!queue_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, queue);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::queue_box::from(queue).set_c_flags(
            memkit::c_api::queue_box::from(queue).c_flags() | QUEUE_FLAG_OWNS_SELF
        );
    }

    *out_queue = queue;
    return QUEUE_OK;
}

void queue_deinit(queue_t* queue)
{
    if (queue == NULL) {
        return;
    }

    memkit::c_api::queue_box::from(queue).deinit();
    memkit::c_api::detail::zero_opaque_bytes(queue, MEMKIT_QUEUE_OBJ_BYTES);
}

void queue_destroy(queue_t* queue)
{
    memkit::c_api::detail::destroy_owned_object<queue_t, memkit::c_api::queue_box>(
        queue,
        QUEUE_FLAG_OWNS_SELF,
        QUEUE_FLAG_DYNAMIC_STORAGE,
        queue_deinit
    );
}

size_t queue_size(const queue_t* queue)
{
    return queue != NULL ? memkit::c_api::queue_box::from(queue).core().size() : 0u;
}

size_t queue_capacity(const queue_t* queue)
{
    return queue != NULL ? memkit::c_api::queue_box::from(queue).core().capacity() : 0u;
}

bool queue_empty(const queue_t* queue)
{
    return queue == NULL || memkit::c_api::queue_box::from(queue).core().empty();
}

bool queue_full(const queue_t* queue)
{
    return queue == NULL || memkit::c_api::queue_box::from(queue).core().full();
}

void queue_clear(queue_t* queue)
{
    if (queue != NULL) {
        memkit::c_api::queue_box::from(queue).core().clear();
    }
}

queue_status_t queue_push(queue_t* queue, const void* elem)
{
    if (queue == NULL) {
        return QUEUE_ERR_NULL;
    }
    return static_cast<queue_status_t>(
        memkit::c_api::queue_box::from(queue).core().push_back(elem)
    );
}

queue_status_t queue_pop(queue_t* queue, void* out_elem)
{
    if (queue == NULL) {
        return QUEUE_ERR_NULL;
    }
    return static_cast<queue_status_t>(
        memkit::c_api::queue_box::from(queue).core().pop_front(out_elem)
    );
}

queue_status_t queue_peek_front(const queue_t* queue, void* out_elem)
{
    if (queue == NULL) {
        return QUEUE_ERR_NULL;
    }
    return static_cast<queue_status_t>(
        memkit::c_api::queue_box::from(queue).core().peek_front(out_elem)
    );
}

queue_status_t queue_peek_back(const queue_t* queue, void* out_elem)
{
    if (queue == NULL) {
        return QUEUE_ERR_NULL;
    }
    return static_cast<queue_status_t>(
        memkit::c_api::queue_box::from(queue).core().peek_back(out_elem)
    );
}

queue_status_t queue_peek_at(const queue_t* queue, size_t index, void* out_elem)
{
    if (queue == NULL) {
        return QUEUE_ERR_NULL;
    }
    return static_cast<queue_status_t>(
        memkit::c_api::queue_box::from(queue).core().peek_at(index, out_elem)
    );
}

queue_status_t queue_foreach(const queue_t* queue, queue_visit_fn visit, void* user)
{
    if (queue == NULL || visit == NULL) {
        return QUEUE_ERR_NULL;
    }

    return static_cast<queue_status_t>(
        memkit::c_api::queue_box::from(queue).core().foreach(
            [visit, user](const void* elem, size_t index) -> memkit::status {
                return static_cast<memkit::status>(visit(elem, index, user));
            }
        )
    );
}

size_t queue_readable_contiguous(const queue_t* queue, const void** out_ptr)
{
    if (queue == NULL) {
        return 0u;
    }
    return memkit::c_api::queue_box::from(queue).core().readable_contiguous(out_ptr);
}

size_t queue_writable_contiguous(queue_t* queue, void** out_ptr)
{
    if (queue == NULL) {
        return 0u;
    }
    return memkit::c_api::queue_box::from(queue).core().writable_contiguous(out_ptr);
}

void queue_commit_read(queue_t* queue, size_t elem_count)
{
    if (queue != NULL) {
        memkit::c_api::queue_box::from(queue).core().commit_read(elem_count);
    }
}

void queue_commit_write(queue_t* queue, size_t elem_count)
{
    if (queue != NULL) {
        memkit::c_api::queue_box::from(queue).core().commit_write(elem_count);
    }
}

} // extern "C"
