# C API reference

Parameter-level reference for the memkit C23 API. For tutorials see [GETTING_STARTED.md](GETTING_STARTED.md); for picking a container see [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md).

**Concurrency:** the C API is **single-context** by default. `queue_t` and `ring_t` are **not** ISR-safe. For cross-context handoff use C++ `SpscQueue` / `MpscQueue` / `DoubleBuffer` or your RTOS primitives — see [CONCURRENCY.md](CONCURRENCY.md).

Headers are authoritative; this document explains **meaning** of shared fields and functions. Tier-1 containers work on MCU; tier-2 require MPU (`MEMKIT_MPU=1`).

### C/C++ parity

| Surface | C API | C++ API |
|---------|-------|---------|
| Arena + 14 containers (ring, queue, vector, …) | **Full** on MPU; tier 1 on MCU | All targets via `memkit.hpp` |
| 18 embedded helpers (lock-free trio, `ByteRing`, `SmallString`, …) | **None** — use C++ | `memkit.hpp` only |

The 14 C-bound containers delegate to the same `detail/*_core` types as C++. Equivalent `*_config_t` / flags / callbacks produce the same behavior as the matching C++ `init` or `init_from_arena`. C++ `try_*` helpers map to status-based `peek`/`pop` in C. Tier-2 C on MCU links but returns `*_ERR_UNSUPPORTED` — use C++ headers on MCU for those containers.

---

## Conventions

### Opaque handles

Each container is:

```c
typedef struct ring {
    alignas(max_align_t) unsigned char bytes[MEMKIT_RING_OBJ_BYTES];
} ring_t;
```

Do not read or write `bytes`. Size macros live in [`memkit_object_sizes.h`](../include/memkit_object_sizes.h).

### Status codes

Every container defines `<name>_status_t` and `<name>_status_ok()`. Common values:

| Code | Meaning |
|------|---------|
| `*_OK` | Success |
| `*_ERR_NULL` | Required pointer argument was NULL |
| `*_ERR_INVALID` | Bad config, size, alignment, or argument |
| `*_ERR_EMPTY` | Pop/peek on empty container |
| `*_ERR_FULL` | Push on full (non-growable, non-overwrite) |
| `*_ERR_OOM` | Allocation failed (arena exhausted, heap disabled, etc.) |
| `*_ERR_UNSUPPORTED` | Tier-2 on MCU, or feature disabled for target |
| `*_ERR_NOT_FOUND` | Lookup miss (maps, cache, pool free of unknown pointer) |

Always check return values (`[[nodiscard]]`).

### Lifecycle: `*_init` vs `*_create`

| API | Handle location | Storage | Teardown |
|-----|-----------------|---------|----------|
| `*_init` | Your struct or stack (`ring_t ring`) | You supply `storage` in config (typical MCU path) | `*_deinit` |
| `*_create` | Output `ring_t **` heap/arena allocated (MPU) | Library or arena allocates | `*_destroy` |

Never `*_init` on a temporary and copy the opaque blob — callback bridges inside the object must stay at a fixed address.

### Element pointers

| Parameter | Direction | Notes |
|-----------|-----------|-------|
| `const void *elem`, `const void *key` | in | Points to a live element/key object |
| `void *out_elem`, `void *out_value` | out | Caller allocates; must hold at least `elem_size` / value size bytes |
| `void **out_ptr` (arena) | out | Receives bump-allocated pointer |

For **trivially copyable** POD types, omit `copy_fn` / `destroy_fn` (memkit uses `memcpy`). For non-trivial types, supply both.

### Callbacks

| Callback | Signature role |
|----------|----------------|
| `*_copy_fn` | `(void *dst, const void *src, void *user)` → status; deep or custom copy into dst |
| `*_destroy_fn` | `(void *elem, void *user)`; run when slot is cleared/evicted |
| `*_visit_fn` | `(const void *elem, size_t index, void *user)` → status; return non-OK to stop iteration |
| `*_compare_fn` | `(const void *a, const void *b, void *user)` → `<0, 0, >0` (pqueue, btree) |
| `*_hash_fn` / `*_key_eq_fn` | Hash map / LRU key hashing and equality |

`user` is an opaque pointer passed to every callback from config.

### Shared config fields (element containers)

Used by `ring_config_t`, `queue_config_t`, `vector_config_t`, `stack_config_t`, `deque_config_t`, `pqueue_config_t`:

| Field | Meaning |
|-------|---------|
| `elem_size` | `sizeof` your element type in bytes |
| `capacity` | Maximum elements in fixed storage |
| `storage` | Caller-owned buffer holding `capacity` elements (`elem_size * capacity` bytes minimum) |
| `storage_bytes` | Size of `storage` in bytes; must be ≥ `elem_size * capacity` |
| `arena` | Optional; used when flags request arena-owned storage or with `*_create` |
| `copy_fn`, `destroy_fn` | Optional element hooks; NULL for POD |
| `user` | Passed to callbacks |
| `flags` | Ownership and behavior bits (see below) |

Use [`memkit_helpers.h`](../include/memkit_helpers.h) macros (`MEMKIT_ELEM_STORAGE`, `MEMKIT_RING_INIT_STATIC`, …) to size storage and fill config.

### Shared ownership flags

Present on most element containers (names prefixed per container, e.g. `RING_FLAG_*`):

| Flag | Meaning |
|------|---------|
| `*_FLAG_NONE` | No extra ownership |
| `*_FLAG_OWNS_STORAGE` | Container frees element storage on `*_deinit` / `*_destroy` |
| `*_FLAG_OWNS_SELF` | Container object itself was heap/arena allocated (`*_create`) |
| `*_FLAG_DYNAMIC_STORAGE` | Element storage came from heap (MPU only) |
| `*_FLAG_ARENA_STORAGE` | Element storage came from bump arena |
| `*_FLAG_GROWABLE` | Double capacity when full (MPU; vector, queue, deque, pqueue, hashmap) |

**Ring only:** `RING_FLAG_OVERWRITE_ON_FULL` — drop oldest when full instead of `*_ERR_FULL`.

**Fixed-capacity maps/lists/pools:** `*_FLAG_FIXED_CAPACITY` — insert fails when pool exhausted (no silent growth).

### Index semantics

For ring-like sequential containers, **`index 0` is the logical front** (oldest queued element). `peek_at` / `foreach` walk front → back.

### DMA-friendly contiguous regions (ring, queue)

| Function | Parameters | Returns / effect |
|----------|------------|------------------|
| `*_readable_contiguous` | `out_ptr`: receives pointer to first readable element run | Count of contiguous elements readable from `*out_ptr` |
| `*_writable_contiguous` | `out_ptr`: receives pointer to writable slot run | Count of contiguous empty slots |
| `*_commit_read` | `elem_count`: elements consumed from front | Advances read index |
| `*_commit_write` | `elem_count`: elements filled | Advances write index |

Use for zero-copy DMA/UART bursts; element type must be trivially readable/writable in place.

---

## Arena (`arena.h`) — tier 1

Bump allocator over a caller buffer or MPU heap/mmap backing.

### `arena_config_t`

| Field | Meaning |
|-------|---------|
| `backing` | Start of arena memory |
| `backing_bytes` | Total arena size |
| `flags` | `ARENA_FLAG_*` ownership of backing |

### `arena_flag_t`

| Flag | Meaning |
|------|---------|
| `ARENA_FLAG_OWNS_BACKING` | `arena_deinit` frees backing |
| `ARENA_FLAG_DYNAMIC_BACKING` | Backing from heap (MPU) |
| `ARENA_FLAG_MMAP_BACKING` | Backing from virtual memory (MPU): POSIX `mmap` or Windows `VirtualAlloc` via `MEMKIT_MEMORY_MMAP` |

### Functions

| Function | Parameters | Purpose |
|----------|------------|---------|
| `arena_init` | `arena`: out live struct; `config`: backing + flags | Embed arena over caller buffer |
| `arena_create` | `arena`: out pointer; `backing_bytes`: size | MPU: allocate arena + default backing |
| `arena_create_with_backing` | `backing`: `USER_BUFFER`, `HEAP`, or `MMAP` | MPU: explicit backing kind (`MMAP` → `mmap` on POSIX, `VirtualAlloc` on Windows) |
| `arena_deinit` | Embedded arena | Release; may free backing if owned |
| `arena_destroy` | Pointer from `arena_create` | Free arena object and owned resources |
| `arena_reset` | — | Invalidate all bumps; O(1) “free all” |
| `arena_alloc` | `size`, `alignment` (power of 2), `out_ptr` | Bump allocate aligned block |
| `arena_calloc` | `count`, `size`, `alignment`, `out_ptr` | Alloc + zero |
| `arena_stats` | `out_stats` | Fill capacity / used / remaining / allocation count |

---

## Ring (`ring.h`) — tier 1

Circular buffer; optional overwrite-oldest policy.

### Extra flags

| Flag | Meaning |
|------|---------|
| `RING_FLAG_OVERWRITE_ON_FULL` | Push drops oldest when full |

### `ring_create`

| Param | Meaning |
|-------|---------|
| `ring` | Out pointer to new ring |
| `elem_size`, `capacity` | Element layout |
| `arena` | May be NULL if heap path; else arena for storage |
| `flags` | Ownership + overwrite policy |

### Operations

| Function | Notes |
|----------|-------|
| `ring_push_back` / `ring_push_front` | `elem` in |
| `ring_pop_front` / `ring_pop_back` | `out_elem` out (may be NULL to discard) |
| `ring_peek_*` | Read without remove |
| `ring_set_at` | Replace at logical index |
| `ring_foreach` | Visit front→back; stop if visit returns ≠ OK |

---

## Queue (`queue.h`) — tier 1

Strict FIFO; fails when full unless growable (MPU).

| Function | Notes |
|----------|-------|
| `queue_push` | Back in |
| `queue_pop` | Front out |
| `queue_peek_front` / `peek_back` / `peek_at` | Non-mutating read |
| Contiguous + commit | Same semantics as ring |

---

## Vector (`vector.h`) — tier 1

Contiguous array; optional growable on MPU.

| Function | Notes |
|----------|-------|
| `vector_reserve` | Ensure capacity ≥ min (may grow on MPU) |
| `vector_push_back` / `pop_back` | End operations |
| `vector_set_at` | Write index (must exist) |
| `vector_at` / `vector_at_const` | Unchecked pointer to element |
| `vector_peek_*` | Same as indexed read |

---

## Stack (`stack.h`) — tier 1

LIFO; type name is `cstack_t` (avoids C `stack` keyword clashes).

| Function | Notes |
|----------|-------|
| `stack_push` | Top in |
| `stack_pop` / `stack_peek` | Top out / read |
| `stack_top` / `stack_top_const` | Pointer to top element |

---

## Deque (`deque.h`) — tier 2 (MPU)

Double-ended queue.

| Function | Notes |
|----------|-------|
| `deque_push_front` / `push_back` | Both ends |
| `deque_pop_front` / `pop_back` | Both ends |
| `deque_front` / `deque_back` | Mutable pointers to ends |

---

## Bitset (`bitset.h`) — tier 1

Fixed bit vector; no `elem_size`.

### `bitset_config_t`

| Field | Meaning |
|-------|---------|
| `capacity` | Number of bits |
| `storage` | Byte buffer; use `bitset_storage_bytes(capacity)` |
| `storage_bytes` | Size of `storage` |
| `arena`, `flags` | As usual |

| Function | Notes |
|----------|-------|
| `bitset_set` / `reset` / `toggle` / `assign` | Single bit |
| `bitset_find_first_set` / `find_first_clear` | `start_index` search start; `out_index` result |
| `bitset_union_with` / `intersect_with` / `xor_with` / `complement` | In-place bitwise ops |
| `bitset_load_bytes` / `store_bytes` | Raw bulk I/O |
| `bitset_data` | Pointer to underlying bytes |

---

## ObjPool (`objpool.h`) — tier 1

Fixed slab; same-sized objects.

### Extra config fields

| Field | Meaning |
|-------|---------|
| `free_stack` | `uint32_t` indices; `objpool_free_stack_bytes(capacity)` |
| `used_bits` | Bitmap of live slots; `objpool_used_bits_bytes(capacity)` |
| `OBJPOOL_FLAG_OWNS_META` | Pool frees free_stack / used_bits metadata |

| Function | Notes |
|----------|-------|
| `objpool_alloc` | `out_elem` receives pointer into slab |
| `objpool_alloc_copy` | Alloc + copy from `src` |
| `objpool_free` | Return slot |
| `objpool_contains` | Pointer in pool and live? |

---

## HandlePool (`handle_pool.h`) — tier 1

Stable `handle_t` (generation-stamped ID) → object.

### Extra config fields

| Field | Meaning |
|-------|---------|
| `generations` | Per-slot generation bytes; `handle_pool_generations_bytes(capacity)` |
| `free_stack` | Free index stack |

| Function | Notes |
|----------|-------|
| `handle_pool_acquire` | `out_elem` + `out_handle`; handle 0 = invalid |
| `handle_pool_release` | Invalidate handle |
| `handle_pool_get` | Resolve handle → element pointer |
| `handle_pool_valid` | Handle still live? |

---

## HashMap (`hashmap.h`) — tier 2 (MPU)

### `hashmap_config_t`

| Field | Meaning |
|-------|---------|
| `key_size`, `value_size` | Fixed entry layout |
| `bucket_count` | Hash buckets or OA slots |
| `strategy` | `CHAINING` or `OPEN_ADDRESSING` |
| `storage` | Caller bucket/slot array |
| `hash_fn`, `key_eq_fn` | Required for byte keys unless using defaults |
| `copy_key_fn`, `copy_value_fn`, `destroy_*` | Optional hooks |

| Function | Notes |
|----------|-------|
| `hashmap_put` | Insert or update |
| `hashmap_get` | `out_value` filled on hit |
| `hashmap_remove` | Delete key |
| `hashmap_open_slot_stride` | Size one OA slot for static storage |

---

## BTree (`btree.h`) — tier 2 (MPU)

Ordered map; nodes from fixed pool.

### `btree_config_t`

| Field | Meaning |
|-------|---------|
| `elem_size` | Packed key+value or node payload size |
| `node_capacity` | Max nodes in pool |
| `node_pool` | Caller node slab |
| `compare_fn` | Required ordering |

| Function | Notes |
|----------|-------|
| `btree_insert` | Add key |
| `btree_get` / `remove` / `contains` | Lookup |
| `btree_peek_min` / `peek_max` | Extremes without remove |
| `btree_foreach` | `traversal`: in/pre/post order |

---

## PQueue (`pqueue.h`) — tier 2 (MPU)

Binary heap priority queue.

| Param | Meaning |
|-------|---------|
| `compare_fn` | `(a,b,user)` → `<0` if `a` higher priority than `b` (min-heap top) |

| Function | Notes |
|----------|-------|
| `pqueue_push` / `pop` / `peek` | Heap operations |
| `pqueue_top` | Pointer to best element |

---

## List / DList (`list.h`, `dlist.h`) — tier 2 (MPU)

Singly / doubly linked lists over a **node pool**.

### `list_config_t`

| Field | Meaning |
|-------|---------|
| `elem_size` | Payload per node |
| `node_capacity` | Max nodes |
| `node_pool` | Byte slab; size ≥ `node_capacity * list_node_stride(elem_size)` |

| Function | Notes |
|----------|-------|
| `list_push_front` / `push_back` | O(1) |
| `list_insert_at` / `remove_at` | Indexed |
| `list_remove_first` | Remove first matching `pred(elem, pred_user)` |
| DList adds `pop_back`, `back`, `foreach_reverse` | — |

---

## LruCache (`lrucache.h`) — tier 2 (MPU)

Fixed-capacity LRU map.

### `lrucache_config_t`

| Field | Meaning |
|-------|---------|
| `capacity` | Max entries |
| `bucket_count` | Hash buckets; helpers size arrays |
| `entry_pool` | Entry structs |
| `buckets` | Pointer array of bucket heads |

| Function | Notes |
|----------|-------|
| `lrucache_get` | Promotes to MRU |
| `lrucache_put` | Insert/update; may evict LRU |
| `lrucache_touch` | Bump existing key |
| `lrucache_foreach_mru` / `foreach_lru` | Iteration order |

---

## Related

- [CXX_API_REFERENCE.md](CXX_API_REFERENCE.md) — C++ templates
- [ADOPTION_GUIDE.md](ADOPTION_GUIDE.md) — integration modes
- [README § C API](../README.md#c-api-1) — overview tables
