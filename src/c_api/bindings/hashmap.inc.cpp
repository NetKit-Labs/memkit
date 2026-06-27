#include "hashmap.h"

#if MEMKIT_C_API_EXTENDED

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/hashmap_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/hash_policy.hpp>
#include <memkit/detail/hashmap_map_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>
#include <cstring>

extern "C" {

size_t hashmap_hash_bytes(const void* key, size_t key_size, void* user)
{
    (void)user;
    return memkit::detail::fnv1a_bytes(key, key_size);
}

bool hashmap_key_equal_bytes(
    const void* a,
    const void* b,
    size_t key_size,
    void* user
)
{
    (void)user;
    return std::memcmp(a, b, key_size) == 0;
}

size_t hashmap_open_slot_stride(size_t key_size, size_t value_size)
{
    return memkit::detail::hashmap_map_core::open_slot_stride(key_size, value_size);
}

hashmap_status_t hashmap_init(hashmap_t* map, const hashmap_config_t* config)
{
    if (map == NULL) {
        return HASHMAP_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(map, MEMKIT_HASHMAP_OBJ_BYTES);

    return memkit::c_api::hashmap_box::from(map).init(config);
}

hashmap_status_t hashmap_create(
    hashmap_t** out_map,
    size_t key_size,
    size_t value_size,
    size_t initial_buckets,
    hashmap_strategy_t strategy,
    arena_t* arena,
    unsigned flags
)
{
    if (out_map == NULL) {
        return HASHMAP_ERR_NULL;
    }
    if (key_size == 0u) {
        return HASHMAP_ERR_INVALID;
    }

    hashmap_t* map = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return HASHMAP_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &map)) {
        return arena == NULL ? HASHMAP_ERR_OOM : HASHMAP_ERR_INVALID;
    }

    hashmap_config_t config = {
        .key_size       = key_size,
        .value_size     = value_size,
        .bucket_count   = initial_buckets,
        .strategy       = strategy,
        .arena          = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | HASHMAP_FLAG_OWNS_STORAGE | HASHMAP_FLAG_GROWABLE,
            arena,
            {
                HASHMAP_FLAG_OWNS_STORAGE,
                HASHMAP_FLAG_DYNAMIC_STORAGE,
                HASHMAP_FLAG_ARENA_STORAGE,
                HASHMAP_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const hashmap_status_t status = hashmap_init(map, &config);
    if (!hashmap_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, map);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::hashmap_box::from(map).set_c_flags(
            memkit::c_api::hashmap_box::from(map).c_flags() | HASHMAP_FLAG_OWNS_SELF
        );
    }

    *out_map = map;
    return HASHMAP_OK;
}

void hashmap_deinit(hashmap_t* map)
{
    if (map == NULL) {
        return;
    }

    memkit::c_api::hashmap_box::from(map).deinit();
    memkit::c_api::detail::zero_opaque_bytes(map, MEMKIT_HASHMAP_OBJ_BYTES);
}

void hashmap_destroy(hashmap_t* map)
{
    memkit::c_api::detail::destroy_owned_object<hashmap_t, memkit::c_api::hashmap_box>(
        map,
        HASHMAP_FLAG_OWNS_SELF,
        HASHMAP_FLAG_DYNAMIC_STORAGE,
        hashmap_deinit
    );
}

hashmap_strategy_t hashmap_strategy_of(const hashmap_t* map)
{
    if (map == NULL) {
        return HASHMAP_STRATEGY_CHAINING;
    }

    const auto strategy = memkit::c_api::hashmap_box::from(map).core().strategy();
    return strategy == memkit::detail::hashmap_strategy::open_addressing
               ? HASHMAP_STRATEGY_OPEN_ADDRESSING
               : HASHMAP_STRATEGY_CHAINING;
}

size_t hashmap_size(const hashmap_t* map)
{
    return map != NULL ? memkit::c_api::hashmap_box::from(map).core().size() : 0u;
}

size_t hashmap_bucket_count(const hashmap_t* map)
{
    return map != NULL ? memkit::c_api::hashmap_box::from(map).core().bucket_count() : 0u;
}

bool hashmap_empty(const hashmap_t* map)
{
    return map == NULL || memkit::c_api::hashmap_box::from(map).core().empty();
}

void hashmap_clear(hashmap_t* map)
{
    if (map == NULL) {
        return;
    }

    memkit::c_api::hashmap_box::from(map).core().clear();
}

hashmap_status_t hashmap_put(hashmap_t* map, const void* key, const void* value)
{
    if (map == NULL || key == NULL || value == NULL) {
        return HASHMAP_ERR_NULL;
    }

    return memkit::c_api::to_hashmap_status(
        memkit::c_api::hashmap_box::from(map).core().put(key, value)
    );
}

hashmap_status_t hashmap_get(const hashmap_t* map, const void* key, void* out_value)
{
    if (map == NULL || key == NULL || out_value == NULL) {
        return HASHMAP_ERR_NULL;
    }

    return memkit::c_api::to_hashmap_status(
        memkit::c_api::hashmap_box::from(map).core().get(key, out_value)
    );
}

hashmap_status_t hashmap_remove(hashmap_t* map, const void* key)
{
    if (map == NULL || key == NULL) {
        return HASHMAP_ERR_NULL;
    }

    return memkit::c_api::to_hashmap_status(
        memkit::c_api::hashmap_box::from(map).core().remove(key)
    );
}

bool hashmap_contains(const hashmap_t* map, const void* key)
{
    if (map == NULL || key == NULL) {
        return false;
    }

    return memkit::c_api::hashmap_box::from(map).core().contains(key);
}

hashmap_status_t hashmap_foreach(const hashmap_t* map, hashmap_visit_fn visit, void* user)
{
    if (map == NULL || visit == NULL) {
        return HASHMAP_ERR_NULL;
    }

    return memkit::c_api::to_hashmap_status(
        memkit::c_api::hashmap_box::from(map).core().foreach(
            [visit, user](const void* key, const void* value) {
                return static_cast<memkit::status>(visit(key, value, user));
            }
        )
    );
}

} // extern "C"

#else

#include <memkit/detail/hash_policy.hpp>
#include <memkit/detail/hashmap_map_core.hpp>

#include <cstddef>
#include <cstdint>

extern "C" {

size_t hashmap_hash_bytes(const void* key, size_t key_size, void* user)
{
    (void)user;
    return memkit::detail::fnv1a_bytes(key, key_size);
}

bool hashmap_key_equal_bytes(
    const void* a,
    const void* b,
    size_t key_size,
    void* user
)
{
    (void)user;
    const auto* left  = static_cast<const std::uint8_t*>(a);
    const auto* right = static_cast<const std::uint8_t*>(b);
    for (size_t i = 0u; i < key_size; ++i) {
        if (left[i] != right[i]) {
            return false;
        }
    }
    return true;
}

size_t hashmap_open_slot_stride(size_t key_size, size_t value_size)
{
    return memkit::detail::hashmap_map_core::open_slot_stride(key_size, value_size);
}

hashmap_status_t hashmap_init(hashmap_t* map, const hashmap_config_t* config)
{
    (void)config;
    if (map == NULL) {
        return HASHMAP_ERR_NULL;
    }
    return HASHMAP_ERR_UNSUPPORTED;
}

hashmap_status_t hashmap_create(
    hashmap_t** out_map,
    size_t key_size,
    size_t value_size,
    size_t initial_buckets,
    hashmap_strategy_t strategy,
    arena_t* arena,
    unsigned flags
)
{
    (void)key_size;
    (void)value_size;
    (void)initial_buckets;
    (void)strategy;
    (void)arena;
    (void)flags;
    if (out_map == NULL) {
        return HASHMAP_ERR_NULL;
    }
    return HASHMAP_ERR_UNSUPPORTED;
}

void hashmap_deinit(hashmap_t* map) { (void)map; }
void hashmap_destroy(hashmap_t* map) { (void)map; }

hashmap_strategy_t hashmap_strategy_of(const hashmap_t* map)
{
    (void)map;
    return HASHMAP_STRATEGY_CHAINING;
}

size_t hashmap_size(const hashmap_t* map) { (void)map; return 0u; }
size_t hashmap_bucket_count(const hashmap_t* map) { (void)map; return 0u; }
bool hashmap_empty(const hashmap_t* map) { return map == NULL || true; }

void hashmap_clear(hashmap_t* map) { (void)map; }

hashmap_status_t hashmap_put(hashmap_t* map, const void* key, const void* value)
{
    (void)key;
    (void)value;
    if (map == NULL) {
        return HASHMAP_ERR_NULL;
    }
    return HASHMAP_ERR_UNSUPPORTED;
}

hashmap_status_t hashmap_get(const hashmap_t* map, const void* key, void* out_value)
{
    (void)key;
    (void)out_value;
    if (map == NULL) {
        return HASHMAP_ERR_NULL;
    }
    return HASHMAP_ERR_UNSUPPORTED;
}

hashmap_status_t hashmap_remove(hashmap_t* map, const void* key)
{
    (void)key;
    if (map == NULL) {
        return HASHMAP_ERR_NULL;
    }
    return HASHMAP_ERR_UNSUPPORTED;
}

bool hashmap_contains(const hashmap_t* map, const void* key)
{
    (void)key;
    (void)map;
    return false;
}

hashmap_status_t hashmap_foreach(const hashmap_t* map, hashmap_visit_fn visit, void* user)
{
    (void)user;
    if (map == NULL || visit == NULL) {
        return HASHMAP_ERR_NULL;
    }
    return HASHMAP_ERR_UNSUPPORTED;
}

} // extern "C"

#endif
