#include "pqueue.h"

#if MEMKIT_C_API_EXTENDED


#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/pqueue_box.hpp>
#include <memkit/c_api/status_cast.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

pqueue_status_t pqueue_init(pqueue_t *pqueue, const pqueue_config_t *config)
{
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(pqueue, MEMKIT_PQUEUE_OBJ_BYTES);

    return memkit::c_api::pqueue_box::from(pqueue).init(config);
}

pqueue_status_t pqueue_create(
    pqueue_t **out_pqueue,
    size_t elem_size,
    pqueue_compare_fn compare_fn,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
)
{
    if (out_pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    if (elem_size == 0u || compare_fn == NULL) {
        return PQUEUE_ERR_INVALID;
    }

    if (initial_capacity == 0u) {
        initial_capacity = 1u;
    }

    pqueue_t *pqueue = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return PQUEUE_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &pqueue)) {
        return arena == NULL ? PQUEUE_ERR_OOM : PQUEUE_ERR_INVALID;
    }

    pqueue_config_t config = {
        .elem_size   = elem_size,
        .capacity    = initial_capacity,
        .arena       = arena,
        .compare_fn  = compare_fn,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | PQUEUE_FLAG_OWNS_STORAGE | PQUEUE_FLAG_GROWABLE,
            arena,
            {
                PQUEUE_FLAG_OWNS_STORAGE,
                PQUEUE_FLAG_DYNAMIC_STORAGE,
                PQUEUE_FLAG_ARENA_STORAGE,
                PQUEUE_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const pqueue_status_t status = pqueue_init(pqueue, &config);
    if (!pqueue_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, pqueue);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::pqueue_box::from(pqueue).set_c_flags(
            memkit::c_api::pqueue_box::from(pqueue).c_flags() | PQUEUE_FLAG_OWNS_SELF
        );
    }

    *out_pqueue = pqueue;
    return PQUEUE_OK;
}

void pqueue_deinit(pqueue_t *pqueue)
{
    if (pqueue == NULL) {
        return;
    }

    memkit::c_api::pqueue_box::from(pqueue).deinit();
    memkit::c_api::detail::zero_opaque_bytes(pqueue, MEMKIT_PQUEUE_OBJ_BYTES);
}

void pqueue_destroy(pqueue_t *pqueue)
{
    memkit::c_api::detail::destroy_owned_object<pqueue_t, memkit::c_api::pqueue_box>(
        pqueue,
        PQUEUE_FLAG_OWNS_SELF,
        PQUEUE_FLAG_DYNAMIC_STORAGE,
        pqueue_deinit
    );
}

size_t pqueue_size(const pqueue_t *pqueue)
{
    return pqueue != NULL ? memkit::c_api::pqueue_box::from(pqueue).core().size() : 0u;
}

size_t pqueue_capacity(const pqueue_t *pqueue)
{
    return pqueue != NULL ? memkit::c_api::pqueue_box::from(pqueue).core().capacity() : 0u;
}

bool pqueue_empty(const pqueue_t *pqueue)
{
    return pqueue == NULL || memkit::c_api::pqueue_box::from(pqueue).core().empty();
}

bool pqueue_full(const pqueue_t *pqueue)
{
    return pqueue != NULL && memkit::c_api::pqueue_box::from(pqueue).core().full();
}

void pqueue_clear(pqueue_t *pqueue)
{
    if (pqueue != NULL) {
        memkit::c_api::pqueue_box::from(pqueue).core().clear();
    }
}

pqueue_status_t pqueue_reserve(pqueue_t *pqueue, size_t min_capacity)
{
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return memkit::c_api::pqueue_box::from(pqueue).reserve(min_capacity);
}

pqueue_status_t pqueue_push(pqueue_t *pqueue, const void *elem)
{
    if (pqueue == NULL || elem == NULL) {
        return PQUEUE_ERR_NULL;
    }

    auto& box = memkit::c_api::pqueue_box::from(pqueue);
    const pqueue_status_t grow_status = box.ensure_capacity(box.core().size() + 1u);
    if (!pqueue_status_ok(grow_status)) {
        return grow_status;
    }

    return memkit::c_api::to_pqueue_status(box.core().push(elem));
}

pqueue_status_t pqueue_pop(pqueue_t *pqueue, void *out_elem)
{
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return memkit::c_api::to_pqueue_status(
        memkit::c_api::pqueue_box::from(pqueue).core().pop(out_elem)
    );
}

pqueue_status_t pqueue_peek(const pqueue_t *pqueue, void *out_elem)
{
    if (pqueue == NULL || out_elem == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return memkit::c_api::to_pqueue_status(
        memkit::c_api::pqueue_box::from(pqueue).core().peek(out_elem)
    );
}

void *pqueue_top(pqueue_t *pqueue)
{
    return pqueue != NULL ? memkit::c_api::pqueue_box::from(pqueue).core().top() : NULL;
}

const void *pqueue_top_const(const pqueue_t *pqueue)
{
    return pqueue != NULL ? memkit::c_api::pqueue_box::from(pqueue).core().top() : NULL;
}

pqueue_status_t pqueue_foreach(const pqueue_t *pqueue, pqueue_visit_fn visit, void *user)
{
    if (pqueue == NULL || visit == NULL) {
        return PQUEUE_ERR_NULL;
    }

    for (std::size_t i = 0u; i < memkit::c_api::pqueue_box::from(pqueue).core().size(); ++i) {
        const void *const slot =
            memkit::c_api::pqueue_box::from(pqueue).core().storage() +
            (i * memkit::c_api::pqueue_box::from(pqueue).core().policy().elem_size());
        const pqueue_status_t status = visit(slot, i, user);
        if (!pqueue_status_ok(status)) {
            return status;
        }
    }

    return PQUEUE_OK;
}

} // extern "C"

#else

extern "C" {

pqueue_status_t pqueue_init(pqueue_t *pqueue, const pqueue_config_t *config)
{
    (void)config;
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return PQUEUE_ERR_UNSUPPORTED;
}

pqueue_status_t pqueue_create(
    pqueue_t **out_pqueue,
    size_t elem_size,
    pqueue_compare_fn compare_fn,
    size_t initial_capacity,
    arena_t *arena,
    unsigned flags
)
{
    (void)elem_size;
    (void)compare_fn;
    (void)initial_capacity;
    (void)arena;
    (void)flags;
    if (out_pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return PQUEUE_ERR_UNSUPPORTED;
}

void pqueue_deinit(pqueue_t *pqueue) { (void)pqueue; }
void pqueue_destroy(pqueue_t *pqueue) { (void)pqueue; }

size_t pqueue_size(const pqueue_t *pqueue) { (void)pqueue; return 0u; }
size_t pqueue_capacity(const pqueue_t *pqueue) { (void)pqueue; return 0u; }
bool pqueue_empty(const pqueue_t *pqueue) { return pqueue == NULL || true; }
bool pqueue_full(const pqueue_t *pqueue) { (void)pqueue; return false; }

void pqueue_clear(pqueue_t *pqueue) { (void)pqueue; }

pqueue_status_t pqueue_reserve(pqueue_t *pqueue, size_t min_capacity)
{
    (void)min_capacity;
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return PQUEUE_ERR_UNSUPPORTED;
}

pqueue_status_t pqueue_push(pqueue_t *pqueue, const void *elem)
{
    (void)elem;
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return PQUEUE_ERR_UNSUPPORTED;
}

pqueue_status_t pqueue_pop(pqueue_t *pqueue, void *out_elem)
{
    (void)out_elem;
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return PQUEUE_ERR_UNSUPPORTED;
}

pqueue_status_t pqueue_peek(const pqueue_t *pqueue, void *out_elem)
{
    (void)out_elem;
    if (pqueue == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return PQUEUE_ERR_UNSUPPORTED;
}

void *pqueue_top(pqueue_t *pqueue) { (void)pqueue; return NULL; }
const void *pqueue_top_const(const pqueue_t *pqueue) { (void)pqueue; return NULL; }

pqueue_status_t pqueue_foreach(const pqueue_t *pqueue, pqueue_visit_fn visit, void *user)
{
    (void)user;
    if (pqueue == NULL || visit == NULL) {
        return PQUEUE_ERR_NULL;
    }
    return PQUEUE_ERR_UNSUPPORTED;
}

} // extern "C"

#endif
