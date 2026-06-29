#include "arena.h"

#include <memkit/detail/utility.hpp>

#include <cstdint>
#include <cstring>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#if MEMKIT_ALLOW_MMAP
#include "memkit/memory/mmap.hpp"
#ifndef _WIN32
#include <sys/mman.h>
#endif
#endif

static bool arena_ptr_is_valid(const arena_t *arena)
{
    return arena != NULL && arena->base != NULL && arena->capacity_bytes > 0u;
}

static arena_status_t arena_validate_config(const arena_config_t *config)
{
    if (config == NULL) {
        return ARENA_ERR_NULL;
    }
    if (config->backing == NULL || config->backing_bytes == 0u) {
        return ARENA_ERR_INVALID;
    }
    return ARENA_OK;
}

static void arena_release_backing(arena_t *arena)
{
    if ((arena->flags & ARENA_FLAG_OWNS_BACKING) == 0u || arena->base == NULL) {
        return;
    }

#if MEMKIT_ALLOW_MMAP
    if ((arena->flags & ARENA_FLAG_MMAP_BACKING) != 0u) {
#ifndef _WIN32
        munmap(arena->base, arena->capacity_bytes);
#endif
        return;
    }
#endif

#if MEMKIT_ALLOW_HEAP
    if ((arena->flags & ARENA_FLAG_DYNAMIC_BACKING) != 0u) {
        std::free(arena->base);
    }
#endif
}

#if MEMKIT_ALLOW_HEAP || MEMKIT_ALLOW_MMAP
static arena_status_t arena_acquire_backing(
    arena_t *arena,
    size_t backing_bytes,
    arena_backing_t backing
)
{
#if MEMKIT_ALLOW_MMAP
    if (backing == ARENA_BACKING_MMAP) {
        memkit::memory::mmap_storage storage =
            memkit::memory::mmap_storage::map(backing_bytes);
        if (!storage.valid()) {
            return ARENA_ERR_OOM;
        }

        const size_t mapped_bytes = storage.size();
        arena->base = reinterpret_cast<uint8_t*>(storage.detach());
        arena->capacity_bytes = mapped_bytes;

        arena->flags |= ARENA_FLAG_OWNS_BACKING | ARENA_FLAG_MMAP_BACKING;
        return ARENA_OK;
    }
#else
    (void)backing;
#endif

#if MEMKIT_ALLOW_HEAP
    if (backing == ARENA_BACKING_HEAP) {
        void *const ptr = std::malloc(backing_bytes);
        if (ptr == NULL) {
            return ARENA_ERR_OOM;
        }

        arena->base = static_cast<uint8_t*>(ptr);
        arena->capacity_bytes = backing_bytes;
        arena->flags |= ARENA_FLAG_OWNS_BACKING | ARENA_FLAG_DYNAMIC_BACKING;
        return ARENA_OK;
    }
#endif

    (void)arena;
    (void)backing_bytes;
    return ARENA_ERR_UNSUPPORTED;
}
#endif /* MEMKIT_ALLOW_HEAP || MEMKIT_ALLOW_MMAP */

extern "C" {

arena_status_t arena_init(arena_t *arena, const arena_config_t *config)
{
    const arena_status_t status = arena_validate_config(config);
    if (!arena_status_ok(status)) {
        return status;
    }
    if (arena == NULL) {
        return ARENA_ERR_NULL;
    }

    *arena = arena_t{
        .base = static_cast<uint8_t*>(config->backing),
        .capacity_bytes = config->backing_bytes,
        .offset_bytes = 0u,
        .allocation_count = 0u,
        .flags = config->flags & (
            ARENA_FLAG_OWNS_BACKING |
            ARENA_FLAG_DYNAMIC_BACKING |
            ARENA_FLAG_MMAP_BACKING
        ),
    };

    return ARENA_OK;
}

static arena_status_t arena_create_impl(
    arena_t **out_arena,
    size_t backing_bytes,
    arena_backing_t backing
)
{
    if (out_arena == NULL) {
        return ARENA_ERR_NULL;
    }
    if (backing_bytes == 0u) {
        return ARENA_ERR_INVALID;
    }

#if MEMKIT_ALLOW_HEAP || MEMKIT_ALLOW_MMAP
    arena_t *const arena = static_cast<arena_t*>(std::malloc(sizeof(*arena)));
    if (arena == NULL) {
        return ARENA_ERR_OOM;
    }

    *arena = arena_t{};

    const arena_status_t backing_status = arena_acquire_backing(arena, backing_bytes, backing);
    if (!arena_status_ok(backing_status)) {
        std::free(arena);
        return backing_status;
    }

    *out_arena = arena;
    return ARENA_OK;
#else
    (void)out_arena;
    (void)backing_bytes;
    (void)backing;
    return ARENA_ERR_UNSUPPORTED;
#endif
}

arena_status_t arena_create(arena_t **out_arena, size_t backing_bytes)
{
    const arena_backing_t backing = static_cast<arena_backing_t>(MEMKIT_DEFAULT_ARENA_BACKING);
    return arena_create_with_backing(out_arena, backing_bytes, backing);
}

arena_status_t arena_create_with_backing(
    arena_t **out_arena,
    size_t backing_bytes,
    arena_backing_t backing
)
{
#if !MEMKIT_ALLOW_MMAP
    if (backing == ARENA_BACKING_MMAP) {
        return ARENA_ERR_UNSUPPORTED;
    }
#endif

#if !MEMKIT_ALLOW_HEAP
    if (backing == ARENA_BACKING_HEAP) {
        return ARENA_ERR_UNSUPPORTED;
    }
#endif

    return arena_create_impl(out_arena, backing_bytes, backing);
}

void arena_deinit(arena_t *arena)
{
    if (arena == NULL) {
        return;
    }

    arena_release_backing(arena);
    *arena = arena_t{};
}

void arena_destroy(arena_t *arena)
{
    if (arena == NULL) {
        return;
    }

    arena_deinit(arena);

#if MEMKIT_ALLOW_HEAP || MEMKIT_ALLOW_MMAP
    std::free(arena);
#endif
}

void arena_reset(arena_t *arena)
{
    if (arena == NULL) {
        return;
    }

    arena->offset_bytes = 0u;
    arena->allocation_count = 0u;
}

arena_status_t arena_alloc(
    arena_t *arena,
    size_t size,
    size_t alignment,
    void **out_ptr
)
{
    if (out_ptr == NULL) {
        return ARENA_ERR_NULL;
    }
    *out_ptr = NULL;

    if (!arena_ptr_is_valid(arena)) {
        return ARENA_ERR_NULL;
    }
    if (size == 0u || alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        return ARENA_ERR_INVALID;
    }
    if (arena->offset_bytes > arena->capacity_bytes) {
        return ARENA_ERR_OOM;
    }

    const uintptr_t base_addr = reinterpret_cast<uintptr_t>(arena->base);
    const uintptr_t raw_addr  = base_addr + arena->offset_bytes;
    if (raw_addr < base_addr) {
        return ARENA_ERR_OOM;
    }

    const uintptr_t aligned_addr = (raw_addr + alignment - 1u) & ~(static_cast<uintptr_t>(alignment) - 1u);
    const size_t aligned_offset = static_cast<size_t>(aligned_addr - base_addr);
    if (aligned_offset < arena->offset_bytes) {
        return ARENA_ERR_OOM;
    }

    if (memkit::detail::add_would_overflow(aligned_offset, size)) {
        return ARENA_ERR_OOM;
    }
    const size_t new_offset = aligned_offset + size;

    if (new_offset > arena->capacity_bytes) {
        return ARENA_ERR_OOM;
    }

    void *const ptr = reinterpret_cast<void*>(aligned_addr);
    arena->offset_bytes = new_offset;
    arena->allocation_count++;

    *out_ptr = ptr;
    return ARENA_OK;
}

arena_status_t arena_calloc(
    arena_t *arena,
    size_t count,
    size_t size,
    size_t alignment,
    void **out_ptr
)
{
    if (count != 0u && memkit::detail::mul_would_overflow(count, size)) {
        return ARENA_ERR_INVALID;
    }

    const size_t total = count * size;
    const arena_status_t status = arena_alloc(arena, total, alignment, out_ptr);
    if (!arena_status_ok(status)) {
        return status;
    }

    std::memset(*out_ptr, 0, total);
    return ARENA_OK;
}

arena_status_t arena_stats(const arena_t *arena, arena_stats_t *out_stats)
{
    if (out_stats == NULL) {
        return ARENA_ERR_NULL;
    }
    if (!arena_ptr_is_valid(arena)) {
        return ARENA_ERR_NULL;
    }
    if (arena->offset_bytes > arena->capacity_bytes) {
        return ARENA_ERR_INVALID;
    }

    *out_stats = arena_stats_t{
        .capacity_bytes = arena->capacity_bytes,
        .used_bytes = arena->offset_bytes,
        .remaining_bytes = arena->capacity_bytes - arena->offset_bytes,
        .allocation_count = arena->allocation_count,
    };

    return ARENA_OK;
}

} // extern "C"
