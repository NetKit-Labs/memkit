#include "deque.h"

#if MEMKIT_C_API_EXTENDED


#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/deque_box.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

deque_status_t deque_init(deque_t* deque, const deque_config_t* config)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(deque, MEMKIT_DEQUE_OBJ_BYTES);

    return memkit::c_api::deque_box::from(deque).init(config);
}

deque_status_t deque_create(
    deque_t** out_deque,
    size_t elem_size,
    size_t initial_capacity,
    arena_t* arena,
    unsigned flags
)
{
    if (out_deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    if (elem_size == 0u) {
        return DEQUE_ERR_INVALID;
    }

    if (initial_capacity == 0u) {
        initial_capacity = 1u;
    }

    deque_t* deque = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return DEQUE_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &deque)) {
        return arena == NULL ? DEQUE_ERR_OOM : DEQUE_ERR_INVALID;
    }

    deque_config_t config = {
        .elem_size = elem_size,
        .capacity = initial_capacity,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | DEQUE_FLAG_OWNS_STORAGE | DEQUE_FLAG_GROWABLE,
            arena,
            {
                DEQUE_FLAG_OWNS_STORAGE,
                DEQUE_FLAG_DYNAMIC_STORAGE,
                DEQUE_FLAG_ARENA_STORAGE,
                DEQUE_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const deque_status_t status = deque_init(deque, &config);
    if (!deque_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, deque);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::deque_box::from(deque).set_c_flags(
            memkit::c_api::deque_box::from(deque).c_flags() | DEQUE_FLAG_OWNS_SELF
        );
    }

    *out_deque = deque;
    return DEQUE_OK;
}

void deque_deinit(deque_t* deque)
{
    if (deque == NULL) {
        return;
    }

    memkit::c_api::deque_box::from(deque).deinit();
    memkit::c_api::detail::zero_opaque_bytes(deque, MEMKIT_DEQUE_OBJ_BYTES);
}

void deque_destroy(deque_t* deque)
{
    memkit::c_api::detail::destroy_owned_object<deque_t, memkit::c_api::deque_box>(
        deque,
        DEQUE_FLAG_OWNS_SELF,
        DEQUE_FLAG_DYNAMIC_STORAGE,
        deque_deinit
    );
}

size_t deque_size(const deque_t* deque)
{
    return deque != NULL ? memkit::c_api::deque_box::from(deque).core().size() : 0u;
}

size_t deque_capacity(const deque_t* deque)
{
    return deque != NULL ? memkit::c_api::deque_box::from(deque).core().capacity() : 0u;
}

bool deque_empty(const deque_t* deque)
{
    return deque == NULL || memkit::c_api::deque_box::from(deque).core().empty();
}

bool deque_full(const deque_t* deque)
{
    return deque == NULL || memkit::c_api::deque_box::from(deque).core().full();
}

void deque_clear(deque_t* deque)
{
    if (deque != NULL) {
        memkit::c_api::deque_box::from(deque).core().clear();
    }
}

deque_status_t deque_reserve(deque_t* deque, size_t min_capacity)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().reserve(min_capacity)
    );
}

deque_status_t deque_push_back(deque_t* deque, const void* elem)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().push_back(elem)
    );
}

deque_status_t deque_push_front(deque_t* deque, const void* elem)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().push_front(elem)
    );
}

deque_status_t deque_pop_front(deque_t* deque, void* out_elem)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().pop_front(out_elem)
    );
}

deque_status_t deque_pop_back(deque_t* deque, void* out_elem)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().pop_back(out_elem)
    );
}

deque_status_t deque_peek_front(const deque_t* deque, void* out_elem)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().peek_front(out_elem)
    );
}

deque_status_t deque_peek_back(const deque_t* deque, void* out_elem)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().peek_back(out_elem)
    );
}

deque_status_t deque_peek_at(const deque_t* deque, size_t index, void* out_elem)
{
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().peek_at(index, out_elem)
    );
}

void* deque_front(deque_t* deque)
{
    return deque != NULL ? memkit::c_api::deque_box::from(deque).core().front() : NULL;
}

const void* deque_front_const(const deque_t* deque)
{
    return deque != NULL ? memkit::c_api::deque_box::from(deque).core().front() : NULL;
}

void* deque_back(deque_t* deque)
{
    return deque != NULL ? memkit::c_api::deque_box::from(deque).core().back() : NULL;
}

const void* deque_back_const(const deque_t* deque)
{
    return deque != NULL ? memkit::c_api::deque_box::from(deque).core().back() : NULL;
}

deque_status_t deque_foreach(const deque_t* deque, deque_visit_fn visit, void* user)
{
    if (deque == NULL || visit == NULL) {
        return DEQUE_ERR_NULL;
    }

    return static_cast<deque_status_t>(
        memkit::c_api::deque_box::from(deque).core().foreach(
            [visit, user](const void* elem, size_t index) -> memkit::status {
                return static_cast<memkit::status>(visit(elem, index, user));
            }
        )
    );
}

} // extern "C"

#else

extern "C" {

deque_status_t deque_init(deque_t *deque, const deque_config_t *config)
{
    (void)config;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_create(
    deque_t **out_deque,
    size_t elem_size,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
)
{
    (void)elem_size;
    (void)initial_capacity;
    (void)arena;
    (void)flags;
    if (out_deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

void deque_deinit(deque_t *deque) { (void)deque; }
void deque_destroy(deque_t *deque) { (void)deque; }

size_t deque_size(const deque_t *deque) { (void)deque; return 0u; }
size_t deque_capacity(const deque_t *deque) { (void)deque; return 0u; }
bool deque_empty(const deque_t *deque) { return deque == NULL || true; }
bool deque_full(const deque_t *deque) { (void)deque; return false; }

void deque_clear(deque_t *deque) { (void)deque; }

deque_status_t deque_reserve(deque_t *deque, size_t min_capacity)
{
    (void)min_capacity;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_push_front(deque_t *deque, const void *elem)
{
    (void)elem;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_push_back(deque_t *deque, const void *elem)
{
    (void)elem;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_pop_front(deque_t *deque, void *out_elem)
{
    (void)out_elem;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_pop_back(deque_t *deque, void *out_elem)
{
    (void)out_elem;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_peek_front(const deque_t *deque, void *out_elem)
{
    (void)out_elem;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_peek_back(const deque_t *deque, void *out_elem)
{
    (void)out_elem;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

deque_status_t deque_peek_at(const deque_t *deque, size_t index, void *out_elem)
{
    (void)index;
    (void)out_elem;
    if (deque == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

void *deque_front(deque_t *deque) { (void)deque; return NULL; }
const void *deque_front_const(const deque_t *deque) { (void)deque; return NULL; }
void *deque_back(deque_t *deque) { (void)deque; return NULL; }
const void *deque_back_const(const deque_t *deque) { (void)deque; return NULL; }

deque_status_t deque_foreach(const deque_t *deque, deque_visit_fn visit, void *user)
{
    (void)user;
    if (deque == NULL || visit == NULL) {
        return DEQUE_ERR_NULL;
    }
    return DEQUE_ERR_UNSUPPORTED;
}

} // extern "C"

#endif
