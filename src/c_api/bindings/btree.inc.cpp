#include "btree.h"

#if MEMKIT_C_API_EXTENDED


#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/btree_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/btree_map_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <cstring>

extern "C" {

int btree_compare_bytes(const void* a, const void* b, size_t elem_size, void* user)
{
    (void)user;
    return std::memcmp(a, b, elem_size);
}

size_t btree_node_stride(size_t elem_size)
{
    return memkit::detail::btree_map_core<
        memkit::detail::runtime_element_policy,
        memkit::detail::runtime_compare_policy>::node_stride(elem_size);
}

btree_status_t btree_init(btree_t* tree, const btree_config_t* config)
{
    if (tree == NULL) {
        return BTREE_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(tree, MEMKIT_BTREE_OBJ_BYTES);

    return memkit::c_api::btree_box::from(tree).init(config);
}

btree_status_t btree_create(
    btree_t** out_tree,
    size_t elem_size,
    btree_compare_fn compare_fn,
    arena_t* arena,
    unsigned flags
)
{
    if (out_tree == NULL) {
        return BTREE_ERR_NULL;
    }
    if (elem_size == 0u || compare_fn == NULL) {
        return BTREE_ERR_INVALID;
    }

    btree_t* tree = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return BTREE_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &tree)) {
        return arena == NULL ? BTREE_ERR_OOM : BTREE_ERR_INVALID;
    }

    btree_config_t config = {
        .elem_size   = elem_size,
        .arena       = arena,
        .compare_fn  = compare_fn,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | BTREE_FLAG_OWNS_STORAGE,
            arena,
            {
                BTREE_FLAG_OWNS_STORAGE,
                BTREE_FLAG_DYNAMIC_STORAGE,
                BTREE_FLAG_ARENA_STORAGE,
                BTREE_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const btree_status_t status = btree_init(tree, &config);
    if (!btree_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, tree);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::btree_box::from(tree).set_c_flags(
            memkit::c_api::btree_box::from(tree).c_flags() | BTREE_FLAG_OWNS_SELF
        );
    }

    *out_tree = tree;
    return BTREE_OK;
}

void btree_deinit(btree_t* tree)
{
    if (tree == NULL) {
        return;
    }

    memkit::c_api::btree_box::from(tree).deinit();
    memkit::c_api::detail::zero_opaque_bytes(tree, MEMKIT_BTREE_OBJ_BYTES);
}

void btree_destroy(btree_t* tree)
{
    memkit::c_api::detail::destroy_owned_object<btree_t, memkit::c_api::btree_box>(
        tree,
        BTREE_FLAG_OWNS_SELF,
        BTREE_FLAG_DYNAMIC_STORAGE,
        btree_deinit
    );
}

size_t btree_size(const btree_t* tree)
{
    return tree != NULL ? memkit::c_api::btree_box::from(tree).core().size() : 0u;
}

size_t btree_capacity(const btree_t* tree)
{
    return tree != NULL ? memkit::c_api::btree_box::from(tree).core().capacity() : 0u;
}

bool btree_empty(const btree_t* tree)
{
    return tree == NULL || memkit::c_api::btree_box::from(tree).core().empty();
}

bool btree_full(const btree_t* tree)
{
    return tree != NULL && memkit::c_api::btree_box::from(tree).core().full();
}

void btree_clear(btree_t* tree)
{
    if (tree != NULL) {
        memkit::c_api::btree_box::from(tree).core().clear();
    }
}

btree_status_t btree_insert(btree_t* tree, const void* elem)
{
    if (tree == NULL || elem == NULL) {
        return BTREE_ERR_NULL;
    }
    return memkit::c_api::to_btree_status(
        memkit::c_api::btree_box::from(tree).core().insert(elem)
    );
}

btree_status_t btree_get(const btree_t* tree, const void* key, void* out_elem)
{
    if (tree == NULL || key == NULL || out_elem == NULL) {
        return BTREE_ERR_NULL;
    }
    return memkit::c_api::to_btree_status(
        memkit::c_api::btree_box::from(tree).core().get(key, out_elem)
    );
}

bool btree_contains(const btree_t* tree, const void* key)
{
    return tree != NULL && key != NULL &&
        memkit::c_api::btree_box::from(tree).core().contains(key);
}

btree_status_t btree_remove(btree_t* tree, const void* key)
{
    if (tree == NULL || key == NULL) {
        return BTREE_ERR_NULL;
    }
    return memkit::c_api::to_btree_status(
        memkit::c_api::btree_box::from(tree).core().remove(key)
    );
}

btree_status_t btree_peek_min(const btree_t* tree, void* out_elem)
{
    if (tree == NULL || out_elem == NULL) {
        return BTREE_ERR_NULL;
    }
    return memkit::c_api::to_btree_status(
        memkit::c_api::btree_box::from(tree).core().peek_min(out_elem)
    );
}

btree_status_t btree_peek_max(const btree_t* tree, void* out_elem)
{
    if (tree == NULL || out_elem == NULL) {
        return BTREE_ERR_NULL;
    }
    return memkit::c_api::to_btree_status(
        memkit::c_api::btree_box::from(tree).core().peek_max(out_elem)
    );
}

btree_status_t btree_foreach(
    const btree_t* tree,
    btree_traversal_t order,
    btree_visit_fn visit,
    void* user
)
{
    if (tree == NULL || visit == NULL) {
        return BTREE_ERR_NULL;
    }

    const memkit::detail::btree_traversal detail_order = [&order]() {
        switch (order) {
        case BTREE_TRAVERSAL_PREORDER:
            return memkit::detail::btree_traversal::preorder;
        case BTREE_TRAVERSAL_POSTORDER:
            return memkit::detail::btree_traversal::postorder;
        default:
            return memkit::detail::btree_traversal::inorder;
        }
    }();

    const memkit::status st = memkit::c_api::btree_box::from(tree).core().foreach(
        detail_order,
        [visit, user](const void* elem) -> memkit::status {
            const btree_status_t status = visit(elem, user);
            return btree_status_ok(status) ? memkit::status::ok : memkit::status::invalid;
        }
    );

    if (st != memkit::status::ok) {
        return BTREE_ERR_INVALID;
    }
    return BTREE_OK;
}

} // extern "C"

#else

extern "C" {

int btree_compare_bytes(const void* a, const void* b, size_t elem_size, void* user)
{
    (void)user;
    const uint8_t* left  = static_cast<const uint8_t*>(a);
    const uint8_t* right = static_cast<const uint8_t*>(b);
    for (size_t i = 0u; i < elem_size; ++i) {
        if (left[i] != right[i]) {
            return left[i] < right[i] ? -1 : 1;
        }
    }
    return 0;
}

size_t btree_node_stride(size_t elem_size)
{
    const size_t header = (sizeof(btree_node_t) + alignof(btree_node_t) - 1u) &
        ~(alignof(btree_node_t) - 1u);
    return header + elem_size;
}

btree_status_t btree_init(btree_t* tree, const btree_config_t* config)
{
    (void)config;
    if (tree == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

btree_status_t btree_create(
    btree_t** out_tree,
    size_t elem_size,
    btree_compare_fn compare_fn,
    arena_t* arena,
    unsigned flags
)
{
    (void)elem_size;
    (void)compare_fn;
    (void)arena;
    (void)flags;
    if (out_tree == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

void btree_deinit(btree_t* tree) { (void)tree; }
void btree_destroy(btree_t* tree) { (void)tree; }

size_t btree_size(const btree_t* tree) { (void)tree; return 0u; }
size_t btree_capacity(const btree_t* tree) { (void)tree; return 0u; }
bool btree_empty(const btree_t* tree) { return tree == NULL || true; }
bool btree_full(const btree_t* tree) { (void)tree; return false; }

void btree_clear(btree_t* tree) { (void)tree; }

btree_status_t btree_insert(btree_t* tree, const void* elem)
{
    (void)elem;
    if (tree == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

btree_status_t btree_get(const btree_t* tree, const void* key, void* out_elem)
{
    (void)key;
    (void)out_elem;
    if (tree == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

bool btree_contains(const btree_t* tree, const void* key)
{
    (void)key;
    (void)tree;
    return false;
}

btree_status_t btree_remove(btree_t* tree, const void* key)
{
    (void)key;
    if (tree == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

btree_status_t btree_peek_min(const btree_t* tree, void* out_elem)
{
    (void)out_elem;
    if (tree == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

btree_status_t btree_peek_max(const btree_t* tree, void* out_elem)
{
    (void)out_elem;
    if (tree == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

btree_status_t btree_foreach(
    const btree_t* tree,
    btree_traversal_t order,
    btree_visit_fn visit,
    void* user
)
{
    (void)order;
    (void)user;
    if (tree == NULL || visit == NULL) {
        return BTREE_ERR_NULL;
    }
    return BTREE_ERR_UNSUPPORTED;
}

} // extern "C"

#endif
