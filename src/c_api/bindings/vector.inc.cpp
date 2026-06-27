#include "vector.h"

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/vector_box.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

vector_status_t vector_init(vector_t* vector, const vector_config_t* config)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(vector, MEMKIT_VECTOR_OBJ_BYTES);

    return memkit::c_api::vector_box::from(vector).init(config);
}

vector_status_t vector_create(
    vector_t** out_vector,
    size_t elem_size,
    size_t initial_capacity,
    arena_t* arena,
    unsigned flags
)
{
    if (out_vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    if (elem_size == 0u) {
        return VECTOR_ERR_INVALID;
    }

    if (initial_capacity == 0u) {
        initial_capacity = 1u;
    }

    vector_t* vector = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return VECTOR_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &vector)) {
        return arena == NULL ? VECTOR_ERR_OOM : VECTOR_ERR_INVALID;
    }

    vector_config_t config = {
        .elem_size = elem_size,
        .capacity = initial_capacity,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | VECTOR_FLAG_OWNS_STORAGE | VECTOR_FLAG_GROWABLE,
            arena,
            {
                VECTOR_FLAG_OWNS_STORAGE,
                VECTOR_FLAG_DYNAMIC_STORAGE,
                VECTOR_FLAG_ARENA_STORAGE,
                VECTOR_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const vector_status_t status = vector_init(vector, &config);
    if (!vector_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, vector);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::vector_box::from(vector).set_c_flags(
            memkit::c_api::vector_box::from(vector).c_flags() | VECTOR_FLAG_OWNS_SELF
        );
    }

    *out_vector = vector;
    return VECTOR_OK;
}

void vector_deinit(vector_t* vector)
{
    if (vector == NULL) {
        return;
    }

    memkit::c_api::vector_box::from(vector).deinit();
    memkit::c_api::detail::zero_opaque_bytes(vector, MEMKIT_VECTOR_OBJ_BYTES);
}

void vector_destroy(vector_t* vector)
{
    memkit::c_api::detail::destroy_owned_object<vector_t, memkit::c_api::vector_box>(
        vector,
        VECTOR_FLAG_OWNS_SELF,
        VECTOR_FLAG_DYNAMIC_STORAGE,
        vector_deinit
    );
}

size_t vector_size(const vector_t* vector)
{
    return vector != NULL ? memkit::c_api::vector_box::from(vector).core().size() : 0u;
}

size_t vector_capacity(const vector_t* vector)
{
    return vector != NULL ? memkit::c_api::vector_box::from(vector).core().capacity() : 0u;
}

bool vector_empty(const vector_t* vector)
{
    return vector == NULL || memkit::c_api::vector_box::from(vector).core().empty();
}

void vector_clear(vector_t* vector)
{
    if (vector != NULL) {
        memkit::c_api::vector_box::from(vector).core().clear();
    }
}

vector_status_t vector_reserve(vector_t* vector, size_t min_capacity)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().reserve(min_capacity)
    );
}

vector_status_t vector_push_back(vector_t* vector, const void* elem)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().push_back(elem)
    );
}

vector_status_t vector_pop_back(vector_t* vector, void* out_elem)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().pop_back(out_elem)
    );
}

vector_status_t vector_peek_front(const vector_t* vector, void* out_elem)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().peek_front(out_elem)
    );
}

vector_status_t vector_peek_back(const vector_t* vector, void* out_elem)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().peek_back(out_elem)
    );
}

vector_status_t vector_peek_at(const vector_t* vector, size_t index, void* out_elem)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().peek_at(index, out_elem)
    );
}

vector_status_t vector_set_at(vector_t* vector, size_t index, const void* elem)
{
    if (vector == NULL) {
        return VECTOR_ERR_NULL;
    }
    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().set_at(index, elem)
    );
}

void* vector_at(vector_t* vector, size_t index)
{
    return vector != NULL
        ? memkit::c_api::vector_box::from(vector).core().at(index)
        : NULL;
}

const void* vector_at_const(const vector_t* vector, size_t index)
{
    return vector != NULL
        ? memkit::c_api::vector_box::from(vector).core().at(index)
        : NULL;
}

vector_status_t vector_foreach(const vector_t* vector, vector_visit_fn visit, void* user)
{
    if (vector == NULL || visit == NULL) {
        return VECTOR_ERR_NULL;
    }

    return static_cast<vector_status_t>(
        memkit::c_api::vector_box::from(vector).core().foreach(
            [visit, user](const void* elem, size_t index) -> memkit::status {
                return static_cast<memkit::status>(visit(elem, index, user));
            }
        )
    );
}

} // extern "C"
