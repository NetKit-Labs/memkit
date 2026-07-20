# C++ API reference

Parameter-level reference for `memkit::` templates in [`include/memkit/memkit.hpp`](../include/memkit/memkit.hpp). For tutorials see [GETTING_STARTED.md](GETTING_STARTED.md); for C API see [C_API_REFERENCE.md](C_API_REFERENCE.md).

---

## Conventions

### Namespace and headers

```cpp
#include <memkit/memkit.hpp>   // all 32 utilities
// or
#include <memkit/containers/ring.hpp>   // one container + dependencies
```

All containers live in namespace `memkit`. Storage helpers: `memkit::memory::`.

### Return types

| Pattern | When | Check |
|---------|------|-------|
| `memkit::status` | `init`, `push`, `pop`, mutating ops | `memkit::ok(st)` |
| `memkit::stl::optional<T>` | `try_pop`, `try_peek` | `.has_value()` |
| `bool` | `empty`, `full`, `contains` | direct |
| Raw pointer | `data_at`, `front` on intrusive types | null check |

### `memkit::status`

```cpp
enum class status : std::uint8_t {
    ok, null_ptr, invalid, empty, full, oom, unsupported, not_found
};
```

Same semantics as C `*_status_t` (see [C_API_REFERENCE.md](C_API_REFERENCE.md)).

### C/C++ parity

Every utility in `memkit.hpp` either has a **C binding** (14 containers + `arena_t`, same `detail/*_core`) or is **C++-only** (18 helpers). See the parity table in [C_API_REFERENCE.md](C_API_REFERENCE.md#cc-parity) and the [README cheat sheet](../README.md#container-cheat-sheet).

### Ownership

- Move-only; no copy constructor.
- Destructors call `clear()` and release owned storage.
- **Caller-owned storage:** `init(array)` / `init(span)` — you keep the buffer alive.
- **Arena-owned:** `init_from_arena` — arena must outlive the container (or until `clear`).
- **Growable (MPU):** policies may reallocate; storage owned by container core until destroy.

---

## Initialization overload matrix

Most sequential containers (`Ring`, `Queue`, `Vector`, `Stack`, `Deque`, `Bitset`, …) support:

| Overload | Parameters | Use when |
|----------|------------|----------|
| `init(stl::array<T, N>& storage)` | Fixed array, capacity = N | Simplest MCU path |
| `init(stl::span<T> storage)` | Typed span | View over caller memory |
| `init(std::byte* storage, size_t capacity)` | Raw bytes + count | Manual layout |
| `init(stl::byte_span storage, size_t capacity)` | Byte span + count | Validates `sizeof(T)*capacity` |
| `init(stl::array<std::byte, N>&, capacity)` | Byte array + logical capacity | Sub-span of buffer |
| `init_from_arena(Arena& arena, size_t capacity, policy)` | Bump arena allocates elements | Shared arena, several containers |

**Requirements:** `T` must satisfy container element requirements (trivially copyable for hot paths; destructors run on `clear` when non-trivial).

**Arena type:** any `memkit::memory::arena<Backing>` or compatible object with `.allocate(size, alignment, void** out)`.

---

## Policy enums

Policies are bitflags or enums passed to `init` / `init_from_arena`. Names mirror C flags where applicable.

| Container | Policy type | Values | Effect |
|-----------|-------------|--------|--------|
| `Ring` | `ring_policy` | `none`, `overwrite_on_full` | Drop oldest when full |
| `Queue` | `queue_policy` | `none`, `growable` | MPU: expand when full |
| `Vector` | `vector_policy` | `none`, `growable` | MPU: expand when full |
| `Stack` | `stack_policy` | same as vector | Shared vector core |
| `Deque` | `deque_policy` | `none`, `growable` | Both ends |
| `HashMap` | `hashmap_policy`, `hashmap_strategy` | growable; chaining / open addressing | — |
| `List` / `DList` | `list_policy` / `dlist_policy` | `fixed_pool`, … | Node pool backing |
| `BTree` | `btree_map_policy` | `fixed_pool` | Fixed node pool |
| `PQueue` | `pqueue_policy` | `growable` | Heap capacity |
| `LruCache` | `lrucache_map_policy` | `fixed_pool` | Fixed entry pool |

Default is fixed capacity unless growable is set (MPU only).

---

## Sequential containers

### `Ring<T>`

| Method | Parameters | Notes |
|--------|------------|-------|
| `push_back` / `push_front` | `const T&` or `T&&` | Overwrite if `overwrite_on_full` policy |
| `pop_front` / `pop_back` | optional `T* out` | Remove |
| `peek_front` / `peek_back` | `T& out` | Status-based peek |
| `try_pop_front` / `try_peek_*` | — | `optional<T>` |
| `readable_contiguous` | `const T** out_ptr` | DMA read window |
| `writable_contiguous` | `T** out_ptr` | DMA write window |
| `commit_read` / `commit_write` | element count | Advance indices |
| `data_at(index)` | logical index 0 = front | nullptr if out of range |

### `Queue<T>`

FIFO: `push_back`, `pop_front`, `peek_front`, contiguous API same as ring. No overwrite policy.

### `Vector<T>`

| Method | Notes |
|--------|-------|
| `push_back` / `pop_back` | End growth |
| `reserve(n)` | Ensure capacity (MPU growable) |
| `at(i)` / `set_at(i, value)` | Indexed access |
| `peek_at` | Read without remove |

### `Stack<T>`

`push`, `pop`, `peek` — LIFO on vector core.

### `Deque<T>`

`push_front`, `push_back`, `pop_front`, `pop_back`, `peek_*`, `front`/`back` pointers.

### `Bitset`

No `T`; `init` takes bit capacity or byte storage. `set`, `reset`, `test`, `toggle`, set algebra, `find_first_*`.

---

## Maps and associative

### `HashMap<K,V,Hash,Eq>`

| Method | Notes |
|--------|-------|
| `put(key, value)` | Insert/update |
| `get(key, out)` / `try_get` | Lookup |
| `remove(key)` | Delete |
| `contains(key)` | bool |
| `foreach(fn)` | Visit all entries |

### `FlatMap<K,V>`

Sorted flat array; `put`/`get`/`remove`; O(log n) lookup.

### `BTree<K,V>`

Ordered map; `insert`, `get`, `remove`, `peek_min`/`max`, `foreach`.

### `EnumMap<Enum,V,N>`

Dense enum-indexed array; `put`, `get`, `at`, `contains`.

### `LruCache<K,V>`

`get`, `put`, `remove`, `touch`, `foreach_mru` / `foreach_lru`.

---

## Pools and handles

### `ObjPool<T>`

`init` with storage + metadata buffers; `alloc` → `T*`, `free`, `contains`.

### `HandlePool<T>`

`acquire` → `(status, handle_t, T*)`; `release(handle)`; `valid`, `get`.

---

## Priority and lists

### `PQueue<T,Compare>`

Min-heap per `Compare`; `push`, `pop`, `peek`.

### `List<T>` / `DList<T>`

Intrusive-node pool; `push_*`, `pop_*`, `insert_at`, `remove_at`, `foreach` (+ reverse on DList).

---

## Embedded-only utilities (C++ only)

**Two-category model:** only **`SpscQueue`**, **`MpscQueue`**, and **`DoubleBuffer`** are lock-free/wait-free cross-context utilities (see [CONCURRENCY.md](CONCURRENCY.md) for ISA requirements — **not supported on Cortex-M0/M0+**). All other types in this table are **single-threaded**; use an OS mutex if shared across RTOS tasks.

| Class | Header | Progress guarantee | Key operations |
|-------|--------|-------------------|----------------|
| `SpscQueue<T>` | `spsc_queue.hpp` | **Wait-free** | `push`, `pop`, `empty`, `full`, `size` — power-of-2 capacity |
| `MpscQueue<T>` | `mpsc_queue.hpp` | **Lock-free** | Multi-producer `push`, single `pop`; aligned storage |
| `DoubleBuffer<T>` | `double_buffer.hpp` | **Wait-free** | `write_span`, `publish`, `read_span` |
| `ByteRing` | `byte_ring.hpp` | Single-threaded | `push_bytes`, contiguous read/write |
| `SmallString<N>` | — | Single-threaded | `assign`, `append`, `view`, `c_str` |
| `SmallBuffer<N>` | — | Single-threaded | `assign`, `data`, `size` |
| `TimerWheel<N>` | `timer_wheel.hpp` | Single-threaded | `schedule(node, ticks)`, `cancel`, `tick` |
| `TokenBucket` | `token_bucket.hpp` | Single-threaded | `refill`, `try_consume`, `consume` |
| `RingLog<Record>` | `ring_log.hpp` | Single-threaded | `append`, `foreach` (newest/oldest order) |
| `SparseSet` | `sparse_set.hpp` | Single-threaded | `insert`, `remove`, `contains`, iterate dense |
| `FixedVariant<Ts...>` | `fixed_variant.hpp` | Single-threaded | `emplace<I>(...)`, `holds`, `get` |
| `FixedIoVec<N>` | `fixed_iovec.hpp` | Single-threaded | `push(ptr,len)`, `slice`, `span` |
| `LookupTable<X,Y>` | `lookup_table.hpp` | Single-threaded | `at`, `lookup` (interpolate) |
| `BitReader` / `BitWriter` | `bit_stream.hpp` | Single-threaded | `read`/`write`, `read_bits`/`write_bits` |
| `MovingAverage<T,N>` / `WindowStats<T,N>` | `running_stats.hpp` | Single-threaded | `push`, `average`, `min`, `max` |
| `IntrusiveListHead` | `intrusive_list.hpp` | Single-threaded | embed `IntrusiveListHook`; `push_back`, `erase`, `splice` |

Many of these require `storage_bytes()` / `storage_align()` static helpers — see header for sizing before `init`.

---

## Memory helpers (`memkit::memory::`)

| Type | Role |
|------|------|
| `fixed_buffer` | Non-owning view over caller `std::array` / bytes |
| `arena<Backing>` | Bump allocator template |
| `static_arena` | `arena<fixed_buffer>` alias |
| `heap_arena` | MPU: arena over `heap_storage` |
| `mmap_arena` | MPU: arena over `mmap_storage` (POSIX `mmap`; Windows `VirtualAlloc`) |
| `mmap_storage` | Page-rounded virtual backing; `map` / `unmap` |

C++ `memory::static_arena` is header-only. C `arena_t` API is in [`arena.h`](../include/arena.h) + `src/arena.cpp`.

Type alias: `memkit::Arena<Backing> = memory::arena<Backing>`.

---

## `try_*` vs status returns

```cpp
if (!memkit::ok(ring.push_back(x))) { /* full, invalid, … */ }

if (auto v = ring.try_peek_front(); v.has_value()) {
    use(*v);
}
```

Prefer `try_*` when empty/full is an expected outcome; use raw `status` when propagating errors up the call stack.

---

## Related

- [CONCURRENCY.md](CONCURRENCY.md) — lock-free trio, RTOS/ISR contract, FreeRTOS patterns
- [C_API_REFERENCE.md](C_API_REFERENCE.md) — C parameter reference
- [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) — which container to pick
- [ADOPTION_GUIDE.md](ADOPTION_GUIDE.md) — vendoring headers vs full library
- [README § C++ API](../README.md#c-api) — summary tables
