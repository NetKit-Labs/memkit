#include "dlist.h"

#if MEMKIT_C_API_EXTENDED


#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/dlist_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/dlist_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

size_t dlist_node_stride(size_t elem_size)
{
    return memkit::detail::dlist_core<memkit::detail::runtime_element_policy>::node_stride(
        elem_size
    );
}

dlist_status_t dlist_init(dlist_t *list, const dlist_config_t *config)
{
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(list, MEMKIT_DLIST_OBJ_BYTES);

    return memkit::c_api::dlist_box::from(list).init(config);
}

dlist_status_t dlist_create(
    dlist_t **out_list,
    size_t elem_size,
    arena_t *arena,
    unsigned flags
)
{
    if (out_list == NULL) {
        return DLIST_ERR_NULL;
    }
    if (elem_size == 0u) {
        return DLIST_ERR_INVALID;
    }

    dlist_t *list = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return DLIST_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &list)) {
        return arena == NULL ? DLIST_ERR_OOM : DLIST_ERR_INVALID;
    }

    dlist_config_t config = {
        .elem_size = elem_size,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | DLIST_FLAG_OWNS_STORAGE,
            arena,
            {
                DLIST_FLAG_OWNS_STORAGE,
                DLIST_FLAG_DYNAMIC_STORAGE,
                DLIST_FLAG_ARENA_STORAGE,
                DLIST_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const dlist_status_t status = dlist_init(list, &config);
    if (!dlist_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, list);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::dlist_box::from(list).set_c_flags(
            memkit::c_api::dlist_box::from(list).c_flags() | DLIST_FLAG_OWNS_SELF
        );
    }

    *out_list = list;
    return DLIST_OK;
}

void dlist_deinit(dlist_t *list)
{
    if (list == NULL) {
        return;
    }

    memkit::c_api::dlist_box::from(list).deinit();
    memkit::c_api::detail::zero_opaque_bytes(list, MEMKIT_DLIST_OBJ_BYTES);
}

void dlist_destroy(dlist_t *list)
{
    memkit::c_api::detail::destroy_owned_object<dlist_t, memkit::c_api::dlist_box>(
        list,
        DLIST_FLAG_OWNS_SELF,
        DLIST_FLAG_DYNAMIC_STORAGE,
        dlist_deinit
    );
}

size_t dlist_size(const dlist_t *list)
{
    return list != NULL ? memkit::c_api::dlist_box::from(list).core().size() : 0u;
}

size_t dlist_capacity(const dlist_t *list)
{
    return list != NULL ? memkit::c_api::dlist_box::from(list).core().capacity() : 0u;
}

bool dlist_empty(const dlist_t *list)
{
    return list == NULL || memkit::c_api::dlist_box::from(list).core().empty();
}

bool dlist_full(const dlist_t *list)
{
    return list != NULL && memkit::c_api::dlist_box::from(list).core().full();
}

void dlist_clear(dlist_t *list)
{
    if (list != NULL) {
        memkit::c_api::dlist_box::from(list).core().clear();
    }
}

dlist_status_t dlist_push_front(dlist_t *list, const void *elem)
{
    if (list == NULL || elem == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().push_front(elem)
    );
}

dlist_status_t dlist_push_back(dlist_t *list, const void *elem)
{
    if (list == NULL || elem == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().push_back(elem)
    );
}

dlist_status_t dlist_pop_front(dlist_t *list, void *out_elem)
{
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().pop_front(out_elem)
    );
}

dlist_status_t dlist_pop_back(dlist_t *list, void *out_elem)
{
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().pop_back(out_elem)
    );
}

dlist_status_t dlist_peek_front(const dlist_t *list, void *out_elem)
{
    if (list == NULL || out_elem == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().peek_front(out_elem)
    );
}

dlist_status_t dlist_peek_back(const dlist_t *list, void *out_elem)
{
    if (list == NULL || out_elem == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().peek_back(out_elem)
    );
}

dlist_status_t dlist_peek_at(const dlist_t *list, size_t index, void *out_elem)
{
    if (list == NULL || out_elem == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().peek_at(index, out_elem)
    );
}

dlist_status_t dlist_insert_at(dlist_t *list, size_t index, const void *elem)
{
    if (list == NULL || elem == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().insert_at(index, elem)
    );
}

dlist_status_t dlist_remove_at(dlist_t *list, size_t index, void *out_elem)
{
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().remove_at(index, out_elem)
    );
}

dlist_status_t dlist_remove_first(
    dlist_t *list,
    dlist_pred_fn pred,
    const void *pred_user,
    void *out_elem
)
{
    if (list == NULL || pred == NULL) {
        return DLIST_ERR_NULL;
    }

    return memkit::c_api::to_dlist_status(
        memkit::c_api::dlist_box::from(list).core().remove_first(
            [pred, pred_user](const void *elem) {
                return pred(elem, pred_user);
            },
            out_elem
        )
    );
}

void *dlist_front(dlist_t *list)
{
    return list != NULL ?
        const_cast<void *>(memkit::c_api::dlist_box::from(list).core().front()) : NULL;
}

const void *dlist_front_const(const dlist_t *list)
{
    return list != NULL ? memkit::c_api::dlist_box::from(list).core().front() : NULL;
}

void *dlist_back(dlist_t *list)
{
    return list != NULL ?
        const_cast<void *>(memkit::c_api::dlist_box::from(list).core().back()) : NULL;
}

const void *dlist_back_const(const dlist_t *list)
{
    return list != NULL ? memkit::c_api::dlist_box::from(list).core().back() : NULL;
}

dlist_status_t dlist_foreach(const dlist_t *list, dlist_visit_fn visit, void *user)
{
    if (list == NULL || visit == NULL) {
        return DLIST_ERR_NULL;
    }

    std::size_t index = 0u;
    const memkit::status st = memkit::c_api::dlist_box::from(list).core().for_each(
        [visit, user, &index](const void *elem, std::size_t) -> memkit::status {
            const dlist_status_t status = visit(elem, index, user);
            ++index;
            return dlist_status_ok(status) ? memkit::status::ok : memkit::status::invalid;
        }
    );

    if (st != memkit::status::ok) {
        return DLIST_ERR_INVALID;
    }
    return DLIST_OK;
}

dlist_status_t dlist_foreach_reverse(
    const dlist_t *list,
    dlist_visit_fn visit,
    void *user
)
{
    if (list == NULL || visit == NULL) {
        return DLIST_ERR_NULL;
    }

    const memkit::status st = memkit::c_api::dlist_box::from(list).core().for_each_reverse(
        [visit, user](const void *elem, std::size_t index) -> memkit::status {
            const dlist_status_t status = visit(elem, index, user);
            return dlist_status_ok(status) ? memkit::status::ok : memkit::status::invalid;
        }
    );

    if (st != memkit::status::ok) {
        return DLIST_ERR_INVALID;
    }
    return DLIST_OK;
}

} // extern "C"

#else

extern "C" {

size_t dlist_node_stride(size_t elem_size)
{
    const size_t header =
        (sizeof(void *) * 2u + alignof(void *) - 1u) & ~(alignof(void *) - 1u);
    return header + elem_size;
}

dlist_status_t dlist_init(dlist_t *list, const dlist_config_t *config)
{
    (void)config;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_create(
    dlist_t **out_list,
    size_t elem_size,
    arena_t *arena,
    unsigned flags
)
{
    (void)elem_size;
    (void)arena;
    (void)flags;
    if (out_list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

void dlist_deinit(dlist_t *list) { (void)list; }
void dlist_destroy(dlist_t *list) { (void)list; }

size_t dlist_size(const dlist_t *list) { (void)list; return 0u; }
size_t dlist_capacity(const dlist_t *list) { (void)list; return 0u; }
bool dlist_empty(const dlist_t *list) { return list == NULL || true; }
bool dlist_full(const dlist_t *list) { (void)list; return false; }

void dlist_clear(dlist_t *list) { (void)list; }

dlist_status_t dlist_push_front(dlist_t *list, const void *elem)
{
    (void)elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_push_back(dlist_t *list, const void *elem)
{
    (void)elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_pop_front(dlist_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_pop_back(dlist_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_peek_front(const dlist_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_peek_back(const dlist_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_peek_at(const dlist_t *list, size_t index, void *out_elem)
{
    (void)index;
    (void)out_elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_insert_at(dlist_t *list, size_t index, const void *elem)
{
    (void)index;
    (void)elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_remove_at(dlist_t *list, size_t index, void *out_elem)
{
    (void)index;
    (void)out_elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_remove_first(
    dlist_t *list,
    dlist_pred_fn pred,
    const void *pred_user,
    void *out_elem
)
{
    (void)pred;
    (void)pred_user;
    (void)out_elem;
    if (list == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

void *dlist_front(dlist_t *list) { (void)list; return NULL; }
const void *dlist_front_const(const dlist_t *list) { (void)list; return NULL; }
void *dlist_back(dlist_t *list) { (void)list; return NULL; }
const void *dlist_back_const(const dlist_t *list) { (void)list; return NULL; }

dlist_status_t dlist_foreach(const dlist_t *list, dlist_visit_fn visit, void *user)
{
    (void)user;
    if (list == NULL || visit == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

dlist_status_t dlist_foreach_reverse(
    const dlist_t *list,
    dlist_visit_fn visit,
    void *user
)
{
    (void)user;
    if (list == NULL || visit == NULL) {
        return DLIST_ERR_NULL;
    }
    return DLIST_ERR_UNSUPPORTED;
}

} // extern "C"

#endif
