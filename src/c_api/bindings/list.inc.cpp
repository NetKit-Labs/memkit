#include "list.h"

#if MEMKIT_C_API_EXTENDED


#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/list_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/list_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

size_t list_node_stride(size_t elem_size)
{
    return memkit::detail::list_core<memkit::detail::runtime_element_policy>::node_stride(
        elem_size
    );
}

list_status_t list_init(list_t *list, const list_config_t *config)
{
    if (list == NULL) {
        return LIST_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(list, MEMKIT_LIST_OBJ_BYTES);

    return memkit::c_api::list_box::from(list).init(config);
}

list_status_t list_create(
    list_t **out_list,
    size_t elem_size,
    arena_t *arena,
    unsigned flags
)
{
    if (out_list == NULL) {
        return LIST_ERR_NULL;
    }
    if (elem_size == 0u) {
        return LIST_ERR_INVALID;
    }

    list_t *list = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return LIST_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &list)) {
        return arena == NULL ? LIST_ERR_OOM : LIST_ERR_INVALID;
    }

    list_config_t config = {
        .elem_size = elem_size,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | LIST_FLAG_OWNS_STORAGE,
            arena,
            {
                LIST_FLAG_OWNS_STORAGE,
                LIST_FLAG_DYNAMIC_STORAGE,
                LIST_FLAG_ARENA_STORAGE,
                LIST_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const list_status_t status = list_init(list, &config);
    if (!list_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, list);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::list_box::from(list).set_c_flags(
            memkit::c_api::list_box::from(list).c_flags() | LIST_FLAG_OWNS_SELF
        );
    }

    *out_list = list;
    return LIST_OK;
}

void list_deinit(list_t *list)
{
    if (list == NULL) {
        return;
    }

    memkit::c_api::list_box::from(list).deinit();
    memkit::c_api::detail::zero_opaque_bytes(list, MEMKIT_LIST_OBJ_BYTES);
}

void list_destroy(list_t *list)
{
    memkit::c_api::detail::destroy_owned_object<list_t, memkit::c_api::list_box>(
        list,
        LIST_FLAG_OWNS_SELF,
        LIST_FLAG_DYNAMIC_STORAGE,
        list_deinit
    );
}

size_t list_size(const list_t *list)
{
    return list != NULL ? memkit::c_api::list_box::from(list).core().size() : 0u;
}

size_t list_capacity(const list_t *list)
{
    return list != NULL ? memkit::c_api::list_box::from(list).core().capacity() : 0u;
}

bool list_empty(const list_t *list)
{
    return list == NULL || memkit::c_api::list_box::from(list).core().empty();
}

bool list_full(const list_t *list)
{
    return list != NULL && memkit::c_api::list_box::from(list).core().full();
}

void list_clear(list_t *list)
{
    if (list != NULL) {
        memkit::c_api::list_box::from(list).core().clear();
    }
}

list_status_t list_push_front(list_t *list, const void *elem)
{
    if (list == NULL || elem == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().push_front(elem)
    );
}

list_status_t list_push_back(list_t *list, const void *elem)
{
    if (list == NULL || elem == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().push_back(elem)
    );
}

list_status_t list_pop_front(list_t *list, void *out_elem)
{
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().pop_front(out_elem)
    );
}

list_status_t list_pop_back(list_t *list, void *out_elem)
{
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().pop_back(out_elem)
    );
}

list_status_t list_peek_front(const list_t *list, void *out_elem)
{
    if (list == NULL || out_elem == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().peek_front(out_elem)
    );
}

list_status_t list_peek_back(const list_t *list, void *out_elem)
{
    if (list == NULL || out_elem == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().peek_back(out_elem)
    );
}

list_status_t list_peek_at(const list_t *list, size_t index, void *out_elem)
{
    if (list == NULL || out_elem == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().peek_at(index, out_elem)
    );
}

list_status_t list_insert_at(list_t *list, size_t index, const void *elem)
{
    if (list == NULL || elem == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().insert_at(index, elem)
    );
}

list_status_t list_remove_at(list_t *list, size_t index, void *out_elem)
{
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().remove_at(index, out_elem)
    );
}

list_status_t list_remove_first(
    list_t *list,
    list_pred_fn pred,
    const void *pred_user,
    void *out_elem
)
{
    if (list == NULL || pred == NULL) {
        return LIST_ERR_NULL;
    }

    return memkit::c_api::to_list_status(
        memkit::c_api::list_box::from(list).core().remove_first(
            [pred, pred_user](const void *elem) {
                return pred(elem, pred_user);
            },
            out_elem
        )
    );
}

void *list_front(list_t *list)
{
    return list != NULL ?
        const_cast<void *>(memkit::c_api::list_box::from(list).core().front()) : NULL;
}

const void *list_front_const(const list_t *list)
{
    return list != NULL ? memkit::c_api::list_box::from(list).core().front() : NULL;
}

list_status_t list_foreach(const list_t *list, list_visit_fn visit, void *user)
{
    if (list == NULL || visit == NULL) {
        return LIST_ERR_NULL;
    }

    std::size_t index = 0u;
    const memkit::status st = memkit::c_api::list_box::from(list).core().for_each(
        [visit, user, &index](const void *elem, std::size_t) -> memkit::status {
            const list_status_t status = visit(elem, index, user);
            ++index;
            return list_status_ok(status) ? memkit::status::ok : memkit::status::invalid;
        }
    );

    if (st != memkit::status::ok) {
        return LIST_ERR_INVALID;
    }
    return LIST_OK;
}

} // extern "C"

#else

extern "C" {

size_t list_node_stride(size_t elem_size)
{
    const size_t header =
        (sizeof(void *) + alignof(void *) - 1u) & ~(alignof(void *) - 1u);
    return header + elem_size;
}

list_status_t list_init(list_t *list, const list_config_t *config)
{
    (void)config;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_create(
    list_t **out_list,
    size_t elem_size,
    arena_t *arena,
    unsigned flags
)
{
    (void)elem_size;
    (void)arena;
    (void)flags;
    if (out_list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

void list_deinit(list_t *list) { (void)list; }
void list_destroy(list_t *list) { (void)list; }

size_t list_size(const list_t *list) { (void)list; return 0u; }
size_t list_capacity(const list_t *list) { (void)list; return 0u; }
bool list_empty(const list_t *list) { return list == NULL || true; }
bool list_full(const list_t *list) { (void)list; return false; }

void list_clear(list_t *list) { (void)list; }

list_status_t list_push_front(list_t *list, const void *elem)
{
    (void)elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_push_back(list_t *list, const void *elem)
{
    (void)elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_pop_front(list_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_pop_back(list_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_peek_front(const list_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_peek_back(const list_t *list, void *out_elem)
{
    (void)out_elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_peek_at(const list_t *list, size_t index, void *out_elem)
{
    (void)index;
    (void)out_elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_insert_at(list_t *list, size_t index, const void *elem)
{
    (void)index;
    (void)elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_remove_at(list_t *list, size_t index, void *out_elem)
{
    (void)index;
    (void)out_elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

list_status_t list_remove_first(
    list_t *list,
    list_pred_fn pred,
    const void *pred_user,
    void *out_elem
)
{
    (void)pred;
    (void)pred_user;
    (void)out_elem;
    if (list == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

void *list_front(list_t *list) { (void)list; return NULL; }
const void *list_front_const(const list_t *list) { (void)list; return NULL; }

list_status_t list_foreach(const list_t *list, list_visit_fn visit, void *user)
{
    (void)user;
    if (list == NULL || visit == NULL) {
        return LIST_ERR_NULL;
    }
    return LIST_ERR_UNSUPPORTED;
}

} // extern "C"

#endif
