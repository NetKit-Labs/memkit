#include "lrucache.h"

#if MEMKIT_C_API_EXTENDED


#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/lrucache_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/lrucache_map_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

size_t lrucache_entry_stride(size_t key_size, size_t value_size)
{
    return memkit::detail::lrucache_map_core<
        memkit::detail::runtime_element_policy,
        memkit::detail::runtime_element_policy>::entry_stride(key_size, value_size);
}

size_t lrucache_entry_pool_bytes(size_t capacity, size_t key_size, size_t value_size)
{
    return memkit::detail::lrucache_map_core<
        memkit::detail::runtime_element_policy,
        memkit::detail::runtime_element_policy>::entry_pool_bytes(capacity, key_size, value_size);
}

size_t lrucache_buckets_bytes(size_t bucket_count)
{
    return memkit::detail::lrucache_map_core<
        memkit::detail::runtime_element_policy,
        memkit::detail::runtime_element_policy>::buckets_bytes(bucket_count);
}

size_t lrucache_default_bucket_count(size_t capacity)
{
    return memkit::detail::lrucache_map_core<
        memkit::detail::runtime_element_policy,
        memkit::detail::runtime_element_policy>::default_bucket_count(capacity);
}

lrucache_status_t lrucache_init(lrucache_t* cache, const lrucache_config_t* config)
{
    if (cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(cache, MEMKIT_LRUCACHE_OBJ_BYTES);

    return memkit::c_api::lrucache_box::from(cache).init(config);
}

lrucache_status_t lrucache_create(
    lrucache_t** out_cache,
    size_t key_size,
    size_t value_size,
    size_t capacity,
    size_t bucket_count,
    arena_t* arena,
    unsigned flags
)
{
    if (out_cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    if (key_size == 0u || capacity == 0u) {
        return LRUCACHE_ERR_INVALID;
    }

    lrucache_t* cache = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return LRUCACHE_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &cache)) {
        return arena == NULL ? LRUCACHE_ERR_OOM : LRUCACHE_ERR_INVALID;
    }

    lrucache_config_t config = {
        .key_size      = key_size,
        .value_size    = value_size,
        .capacity      = capacity,
        .bucket_count  = bucket_count,
        .arena         = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | LRUCACHE_FLAG_OWNS_STORAGE,
            arena,
            {
                LRUCACHE_FLAG_OWNS_STORAGE,
                LRUCACHE_FLAG_DYNAMIC_STORAGE,
                LRUCACHE_FLAG_ARENA_STORAGE,
                LRUCACHE_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const lrucache_status_t status = lrucache_init(cache, &config);
    if (!lrucache_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, cache);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::lrucache_box::from(cache).set_c_flags(
            memkit::c_api::lrucache_box::from(cache).c_flags() | LRUCACHE_FLAG_OWNS_SELF
        );
    }

    *out_cache = cache;
    return LRUCACHE_OK;
}

void lrucache_deinit(lrucache_t* cache)
{
    if (cache == NULL) {
        return;
    }

    memkit::c_api::lrucache_box::from(cache).deinit();
    memkit::c_api::detail::zero_opaque_bytes(cache, MEMKIT_LRUCACHE_OBJ_BYTES);
}

void lrucache_destroy(lrucache_t* cache)
{
    memkit::c_api::detail::destroy_owned_object<lrucache_t, memkit::c_api::lrucache_box>(
        cache,
        LRUCACHE_FLAG_OWNS_SELF,
        LRUCACHE_FLAG_DYNAMIC_STORAGE,
        lrucache_deinit
    );
}

size_t lrucache_size(const lrucache_t* cache)
{
    return cache != NULL ? memkit::c_api::lrucache_box::from(cache).core().size() : 0u;
}

size_t lrucache_capacity(const lrucache_t* cache)
{
    return cache != NULL ? memkit::c_api::lrucache_box::from(cache).core().capacity() : 0u;
}

size_t lrucache_bucket_count(const lrucache_t* cache)
{
    return cache != NULL ? memkit::c_api::lrucache_box::from(cache).core().bucket_count() : 0u;
}

bool lrucache_empty(const lrucache_t* cache)
{
    return cache == NULL || memkit::c_api::lrucache_box::from(cache).core().empty();
}

bool lrucache_full(const lrucache_t* cache)
{
    return cache != NULL && memkit::c_api::lrucache_box::from(cache).core().full();
}

void lrucache_clear(lrucache_t* cache)
{
    if (cache != NULL) {
        memkit::c_api::lrucache_box::from(cache).core().clear();
    }
}

lrucache_status_t lrucache_get(lrucache_t* cache, const void* key, void* out_value)
{
    if (cache == NULL || key == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return memkit::c_api::to_lrucache_status(
        memkit::c_api::lrucache_box::from(cache).core().get(key, out_value)
    );
}

lrucache_status_t lrucache_peek(const lrucache_t* cache, const void* key, void* out_value)
{
    if (cache == NULL || key == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return memkit::c_api::to_lrucache_status(
        memkit::c_api::lrucache_box::from(cache).core().peek(key, out_value)
    );
}

lrucache_status_t lrucache_put(lrucache_t* cache, const void* key, const void* value)
{
    if (cache == NULL || key == NULL || value == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return memkit::c_api::to_lrucache_status(
        memkit::c_api::lrucache_box::from(cache).core().put(key, value)
    );
}

lrucache_status_t lrucache_remove(lrucache_t* cache, const void* key)
{
    if (cache == NULL || key == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return memkit::c_api::to_lrucache_status(
        memkit::c_api::lrucache_box::from(cache).core().remove(key)
    );
}

bool lrucache_contains(const lrucache_t* cache, const void* key)
{
    return cache != NULL && key != NULL &&
        memkit::c_api::lrucache_box::from(cache).core().contains(key);
}

lrucache_status_t lrucache_touch(lrucache_t* cache, const void* key)
{
    if (cache == NULL || key == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return memkit::c_api::to_lrucache_status(
        memkit::c_api::lrucache_box::from(cache).core().touch(key)
    );
}

lrucache_status_t lrucache_foreach_mru(
    const lrucache_t* cache,
    lrucache_visit_fn visit,
    void* user
)
{
    if (cache == NULL || visit == NULL) {
        return LRUCACHE_ERR_NULL;
    }

    const memkit::status st = memkit::c_api::lrucache_box::from(cache).core().foreach_mru(
        [visit, user](const void* key, const void* value) -> memkit::status {
            const lrucache_status_t status = visit(key, value, user);
            return lrucache_status_ok(status) ? memkit::status::ok : memkit::status::invalid;
        }
    );

    if (st != memkit::status::ok) {
        return LRUCACHE_ERR_INVALID;
    }
    return LRUCACHE_OK;
}

lrucache_status_t lrucache_foreach_lru(
    const lrucache_t* cache,
    lrucache_visit_fn visit,
    void* user
)
{
    if (cache == NULL || visit == NULL) {
        return LRUCACHE_ERR_NULL;
    }

    const memkit::status st = memkit::c_api::lrucache_box::from(cache).core().foreach_lru(
        [visit, user](const void* key, const void* value) -> memkit::status {
            const lrucache_status_t status = visit(key, value, user);
            return lrucache_status_ok(status) ? memkit::status::ok : memkit::status::invalid;
        }
    );

    if (st != memkit::status::ok) {
        return LRUCACHE_ERR_INVALID;
    }
    return LRUCACHE_OK;
}

} // extern "C"

#else

extern "C" {

size_t lrucache_entry_stride(size_t key_size, size_t value_size)
{
    const size_t header = (sizeof(lrucache_entry_t) + alignof(lrucache_entry_t) - 1u) &
        ~(alignof(lrucache_entry_t) - 1u);
    return header + key_size + value_size;
}

size_t lrucache_entry_pool_bytes(size_t capacity, size_t key_size, size_t value_size)
{
    return capacity * lrucache_entry_stride(key_size, value_size);
}

size_t lrucache_buckets_bytes(size_t bucket_count)
{
    return bucket_count * sizeof(lrucache_entry_t*);
}

size_t lrucache_default_bucket_count(size_t capacity)
{
    size_t buckets = 4u;
    while (buckets < capacity) {
        if (buckets > SIZE_MAX / 2u) {
            return capacity;
        }
        buckets *= 2u;
    }
    return buckets;
}

lrucache_status_t lrucache_init(lrucache_t* cache, const lrucache_config_t* config)
{
    (void)config;
    if (cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

lrucache_status_t lrucache_create(
    lrucache_t** out_cache,
    size_t key_size,
    size_t value_size,
    size_t capacity,
    size_t bucket_count,
    arena_t* arena,
    unsigned flags
)
{
    (void)key_size;
    (void)value_size;
    (void)capacity;
    (void)bucket_count;
    (void)arena;
    (void)flags;
    if (out_cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

void lrucache_deinit(lrucache_t* cache) { (void)cache; }
void lrucache_destroy(lrucache_t* cache) { (void)cache; }

size_t lrucache_size(const lrucache_t* cache) { (void)cache; return 0u; }
size_t lrucache_capacity(const lrucache_t* cache) { (void)cache; return 0u; }
size_t lrucache_bucket_count(const lrucache_t* cache) { (void)cache; return 0u; }
bool lrucache_empty(const lrucache_t* cache) { return cache == NULL || true; }
bool lrucache_full(const lrucache_t* cache) { (void)cache; return false; }

void lrucache_clear(lrucache_t* cache) { (void)cache; }

lrucache_status_t lrucache_get(lrucache_t* cache, const void* key, void* out_value)
{
    (void)key;
    (void)out_value;
    if (cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

lrucache_status_t lrucache_peek(const lrucache_t* cache, const void* key, void* out_value)
{
    (void)key;
    (void)out_value;
    if (cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

lrucache_status_t lrucache_put(lrucache_t* cache, const void* key, const void* value)
{
    (void)key;
    (void)value;
    if (cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

lrucache_status_t lrucache_remove(lrucache_t* cache, const void* key)
{
    (void)key;
    if (cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

bool lrucache_contains(const lrucache_t* cache, const void* key)
{
    (void)key;
    (void)cache;
    return false;
}

lrucache_status_t lrucache_touch(lrucache_t* cache, const void* key)
{
    (void)key;
    if (cache == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

lrucache_status_t lrucache_foreach_mru(
    const lrucache_t* cache,
    lrucache_visit_fn visit,
    void* user
)
{
    (void)user;
    if (cache == NULL || visit == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

lrucache_status_t lrucache_foreach_lru(
    const lrucache_t* cache,
    lrucache_visit_fn visit,
    void* user
)
{
    (void)user;
    if (cache == NULL || visit == NULL) {
        return LRUCACHE_ERR_NULL;
    }
    return LRUCACHE_ERR_UNSUPPORTED;
}

} // extern "C"

#endif
