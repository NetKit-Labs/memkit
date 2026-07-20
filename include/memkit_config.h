#ifndef MEMKIT_CONFIG_H
#define MEMKIT_CONFIG_H

/*
 * memkit build-time memory and platform policy.
 *
 * Targets
 *   MCU (bare-metal): zero-cost STL only; no heap, no mmap, no heap STL via memkit.
 *   MPU (embedded Linux, macOS, Windows host): heap and optional virtual backing;
 *     heap STL optional (MEMKIT_USE_STL). EMBEDDED_LINUX=1 on Linux; optional elsewhere.
 *
 * Zero-cost STL (std::array, std::span, std::optional, std::less, …) is always
 * available via include/memkit/stl.hpp on both MCU and MPU.
 *
 * Memory models
 *   MEMKIT_MEMORY_FIXED_BUFFER  caller-owned static storage
 *   MEMKIT_MEMORY_FIXED_POOL    fixed-size object slab
 *   MEMKIT_MEMORY_ARENA         bump allocator (default backing for both targets)
 *   MEMKIT_MEMORY_HEAP          malloc/free (MPU only)
 *   MEMKIT_MEMORY_MMAP          virtual-memory arena backing (MPU only: mmap or VirtualAlloc)
 *
 * Design rationale: docs/DESIGN_PHILOSOPHY.md
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum memkit_memory_model {
    MEMKIT_MEMORY_FIXED_BUFFER = 0,
    MEMKIT_MEMORY_FIXED_POOL   = 1,
    MEMKIT_MEMORY_ARENA        = 2,
    MEMKIT_MEMORY_HEAP         = 3,
    MEMKIT_MEMORY_MMAP         = 4,
} memkit_memory_model_t;

#ifdef __cplusplus
}
#endif

/* --- Platform detection --------------------------------------------------- */

#if (defined(MEMKIT_MPU) && MEMKIT_MPU) || (defined(EMBEDDED_LINUX) && EMBEDDED_LINUX)
#define MEMKIT_TARGET_MPU 1
#define MEMKIT_TARGET_MCU 0
#else
#define MEMKIT_TARGET_MPU 0
#define MEMKIT_TARGET_MCU 1
#endif

/* --- Feature gates -------------------------------------------------------- */

#if MEMKIT_TARGET_MPU

#ifndef MEMKIT_ALLOW_HEAP
#define MEMKIT_ALLOW_HEAP 1
#endif

#ifndef MEMKIT_ALLOW_MMAP
#define MEMKIT_ALLOW_MMAP 1
#endif

#ifndef MEMKIT_DEFAULT_ARENA_BACKING
#define MEMKIT_DEFAULT_ARENA_BACKING MEMKIT_MEMORY_MMAP
#endif

#ifndef MEMKIT_USE_STL
#define MEMKIT_USE_STL 0
#endif

#else /* MEMKIT_TARGET_MCU */

#ifndef MEMKIT_ALLOW_HEAP
#define MEMKIT_ALLOW_HEAP 0
#endif

#ifndef MEMKIT_ALLOW_MMAP
#define MEMKIT_ALLOW_MMAP 0
#endif

#ifndef MEMKIT_DEFAULT_ARENA_BACKING
#define MEMKIT_DEFAULT_ARENA_BACKING MEMKIT_MEMORY_FIXED_BUFFER
#endif

#ifndef MEMKIT_USE_STL
#define MEMKIT_USE_STL 0
#endif

#endif /* MEMKIT_TARGET_MPU / MEMKIT_TARGET_MCU */

/* --- C API tiering -------------------------------------------------------- */
/*
 * The C API is split into two tiers to keep MCU firmware images small.
 *
 * Tier 1 (MEMKIT_C_API_FULL): always enabled on every target.
 *   ring, vector, stack, queue, bitset, objpool, handle_pool, arena
 *
 * Tier 2 (MEMKIT_C_API_EXTENDED): enabled on MPU; stubbed on MCU.
 *   hashmap, btree, pqueue, list, dlist, lrucache, deque
 *
 * When MEMKIT_C_API_EXTENDED is 0, tier-2 headers still declare the API but
 * init/create return *_ERR_UNSUPPORTED and accessors are no-ops so links succeed.
 *
 * C++ users: include <memkit/memkit.hpp> for all containers on any target.
 * C users on MCU firmware: use tier-1 C API only; prefer C++ memkit.hpp when
 * you need tier-2 containers.
 */
#define MEMKIT_C_API_FULL 1

#if MEMKIT_TARGET_MPU
#define MEMKIT_C_API_EXTENDED 1
#else
#define MEMKIT_C_API_EXTENDED 0
#endif

/* --- STL policy ----------------------------------------------------------- */
/*
 * MCU: zero-cost STL only (array, span, optional, less, …). Heap STL is not
 * exposed via memkit. Programmers may use std::vector/string on their own.
 * MPU: optional heap STL aliases when MEMKIT_USE_STL=1.
 */
#if MEMKIT_TARGET_MCU
#if MEMKIT_USE_STL
#error "MEMKIT_USE_STL is not supported on MCU; memkit exposes zero-cost STL only"
#endif
#undef MEMKIT_USE_STL
#define MEMKIT_USE_STL 0
#define MEMKIT_ALLOW_HEAP_STL 0
#elif MEMKIT_TARGET_MPU && MEMKIT_USE_STL
#define MEMKIT_ALLOW_HEAP_STL 1
#else
#define MEMKIT_ALLOW_HEAP_STL 0
#endif

#ifndef MEMKIT_ALLOW_HEAP_STL
#define MEMKIT_ALLOW_HEAP_STL 0
#endif

#endif /* MEMKIT_CONFIG_H */
