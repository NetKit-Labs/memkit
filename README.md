# memkit

Embedded-friendly containers for C and C++ with a single shared implementation per container family. C++ templates are the primary API; the C API is a thin, type-erased layer over the same cores.

## Quick start

```bash
make all              # lib + 15 C++ tests + MCU example
make test_c_api_smoke # C API smoke test (MCU)
make mpu              # MPU examples + extended C API test
make clean
```

**C++ — static ring on MCU:**

```cpp
#include <memkit/memkit.hpp>

memkit::stl::array<int, 8> storage{};
memkit::Ring<int> ring;
ring.init(storage);
ring.push_back(42);
```

**C — static ring on MCU:**

```c
#include <ring.h>

static uint8_t storage[sizeof(int) * 8];
ring_t ring;
ring_init(&ring, &(ring_config_t){
    .elem_size = sizeof(int), .capacity = 8,
    .storage = storage, .storage_bytes = sizeof storage,
});
int value = 42;
ring_push_back(&ring, &value);
ring_deinit(&ring);
```

See [Build](#build), [C++ API](#c-api), and [C API](#c-api-1) below for full details.

## Design

```
C++ Ring<T>, Vector<T>, …  →  detail/*_core<Policy>  ←  c_api/*_box  →  src/c_api/bindings.cpp
                                      ↑
                         typed_element_policy<T>  |  runtime_element_policy (C)
```

- **One algorithm per family.** Ring, queue, and deque share `ring_buffer_core`. Stack reuses `vector_core`. HashMap, BTree, and LruCache share `*_map_core`.
- **No duplicate logic.** C++ uses `typed_element_policy<T>`; C uses `runtime_element_policy` with `elem_size` and optional `copy_fn` / `destroy_fn`.
- **Opaque C objects.** Each C container is a fixed-size blob (`unsigned char bytes[MEMKIT_*_OBJ_BYTES]`) verified at library build time via `include/memkit/c_api/static_checks.hpp`.
- **Slim C API build.** All `extern "C"` bindings compile in one translation unit (`src/c_api/bindings.cpp`), which `#include`s per-container fragments under `src/c_api/bindings/*.inc.cpp` (those fragments are part of the same compile — not separate or dead code). Callback bridges and layout checks are header-only.

**C++ (16 containers)** — all templates or header-only classes in `include/memkit/containers/`, included via `<memkit/memkit.hpp>`.

**C (14 containers)** — type-erased by design (C23 has no generics). Each container exposes `*_init` / `*_create` / `*_destroy` over a shared `*_box` implementation. `SmallString` and `ByteRing` are C++-only.

Pick the API that fits your project:

| Use case | API |
|----------|-----|
| Firmware (C), small image, tier-1 containers | C API tier 1 |
| Firmware (C++), all containers, static/arena storage | `#include <memkit/memkit.hpp>` |
| Embedded Linux (C or C++), heap/mmap | Either API; tier 2 available in C |
| Mixed C/C++ codebase | C API from C; C++ templates from C++ |

## Targets

memkit distinguishes two build targets (see `include/memkit_config.h`):

| | **MCU** (bare-metal) | **MPU** (embedded Linux) |
|--|----------------------|--------------------------|
| Default define | `MEMKIT_MCU=1` | `MEMKIT_MPU=1`, `EMBEDDED_LINUX=1` |
| Heap (`malloc`) | off | on (`MEMKIT_ALLOW_HEAP=1`) |
| mmap arena | off | on (`MEMKIT_ALLOW_MMAP=1`) |
| Default arena backing | caller buffer | mmap |
| C API tier 2 | stubbed (`*_ERR_UNSUPPORTED`) | full |
| C++ containers | all | all |
| Heap STL via memkit | no (`MEMKIT_ALLOW_HEAP_STL=0`) | optional (`MEMKIT_USE_STL=1` → `memkit::stl::vector`, `string`) |

**MCU** builds are sized for firmware: no heap inside memkit, zero-cost STL only (`std::array`, `std::span`, `std::optional`, `std::string_view`, … via `memkit/stl.hpp`). Setting `MEMKIT_USE_STL=1` on MCU is a compile error.

**MPU** builds add heap allocation, mmap-backed arenas, and the full C API. Optional heap STL aliases (`memkit::stl::vector`, `memkit::stl::string`) are available only when `MEMKIT_USE_STL=1` (CMake: `-DMEMKIT_USE_STL=ON`). Core containers never use heap STL internally.

## Memory models

Containers can store elements in several ways. The same options exist in C (flags + config) and C++ (`init`, `init_from_arena`, policies).

| Model | C flag / config | C++ backing | MCU | MPU |
|-------|-----------------|-------------|-----|-----|
| Fixed buffer | caller `storage` in `*_config` | `stl::array<T,N>`, `stl::span<T>`, raw bytes | yes | yes |
| Fixed pool | slab / pool storage | `FixedPool<…>` | yes | yes |
| Arena | `arena_t *` + `*_FLAG_ARENA_STORAGE` | `init_from_arena(arena, …)` | yes | yes |
| Heap | `*_FLAG_DYNAMIC_STORAGE` (create helpers) | heap arena / growable | no | yes |
| mmap | arena with mmap backing | `memory::mmap_arena` | no | yes |

**Arena** is a bump allocator: fast sequential allocation, reset the whole arena in one call. Good for short-lived batches of container storage.

**Growable** (vector, stack, queue, deque, pqueue, hashmap): doubles capacity when full; on MPU may use heap if no arena is supplied.

## Build

The default Makefile target builds the MCU library and C++ tests:

```bash
make all              # lib + 15 C++ tests + example_mcu
make test_cpp         # C++ container tests only
make test_c_api_smoke # minimal C API smoke test (MCU)
make mpu              # MPU: example_mpu + example_mpu_c + test_c_api_extended
make clean
```

### CMake

```bash
cmake -B build                           # MCU (default)
cmake -B build -DMEMKIT_EMBEDDED_LINUX=ON   # MPU
cmake --build build
ctest --test-dir build
```

MPU builds also produce `example_mpu` (C++) and `example_mpu_c` (C).

Link against the static library built from `src/arena.cpp`, `src/mmap_backing.cpp`, and `src/c_api/bindings.cpp`. Add `-Iinclude` and `-DMEMKIT_MCU=1` or `-DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1`.

---

## C++ API

Include the umbrella header:

```cpp
#include <memkit/memkit.hpp>
```

All containers live in namespace `memkit`. Operations return `memkit::status`; use `memkit::ok(st)` to test success.

### Containers

| Class | Header (via memkit.hpp) | Role |
|-------|-------------------------|------|
| `Ring<T>` | `containers/ring.hpp` | Circular buffer; optional overwrite-on-full |
| `Queue<T>` | `containers/queue.hpp` | FIFO ring |
| `Deque<T>` | `containers/deque.hpp` | Double-ended ring |
| `Vector<T>` | `containers/vector.hpp` | Contiguous array; optional growable |
| `Stack<T>` | `containers/stack.hpp` | LIFO (vector core) |
| `Bitset` | `containers/bitset.hpp` | Fixed bit set |
| `ObjPool<T>` | `containers/objpool.hpp` | Fixed-size object pool |
| `HashMap<K,V>` | `containers/hashmap.hpp` | Hash map (chaining or open addressing) |
| `BTree<K,V>` | `containers/btree.hpp` | Ordered map |
| `PQueue<T,Compare>` | `containers/pqueue.hpp` | Binary heap priority queue |
| `List<T>` | `containers/list.hpp` | Singly linked list |
| `DList<T>` | `containers/dlist.hpp` | Doubly linked list |
| `LruCache<K,V>` | `containers/lrucache.hpp` | LRU cache |
| `HandlePool<T>` | `containers/handle_pool.hpp` | Generation-based stable handles |
| `SmallString<N>` | `containers/small_string.hpp` | Fixed-capacity string (no heap) |
| `ByteRing` | `containers/byte_ring.hpp` | Byte stream ring for UART/DMA I/O |

Memory helpers: `memkit::memory::static_arena`, `fixed_buffer`, `fixed_pool`, and on MPU `heap_arena`, `mmap_arena`, `mmap_storage`.

Type aliases: `memkit::Arena<…>`, `memkit::FixedPool<…>`.

### Typical MCU pattern (static storage)

From `examples/example_mcu.cpp`:

```cpp
#include <memkit/memkit.hpp>

struct sensor_sample { std::uint32_t ts; std::int16_t value; };

memkit::stl::array<sensor_sample, 16> ring_storage{};
memkit::Ring<sensor_sample> queue;

assert(memkit::ok(queue.init(ring_storage)));

sensor_sample s{100, 42};
assert(memkit::ok(queue.push_back(s)));

auto oldest = queue.try_peek_front();  // std::optional
```

Use `memkit::stl::array`, `memkit::stl::span`, and `memkit::stl::optional` instead of pulling in heap STL through memkit on MCU.

### Arena-backed container

```cpp
memkit::stl::array<std::byte, 512> arena_backing{};
memkit::memory::fixed_buffer backing{arena_backing};
memkit::memory::static_arena arena{backing};

memkit::Ring<sensor_sample> log;
assert(memkit::ok(log.init_from_arena(
    arena, 8u, memkit::ring_policy::overwrite_on_full)));
```

### SmallString and ByteRing (MCU-friendly utilities)

```cpp
memkit::SmallString<32> label;
label.assign("sensor-A");
label.append("-1");

memkit::stl::array<std::byte, 256> rx_buf{};
memkit::ByteRing uart;
uart.init(rx_buf, 128u);
uart.push_bytes(data, len);
const std::uint8_t* chunk = nullptr;
uart.readable_contiguous(&chunk);
uart.commit_read(n);
```

### Initialization summary (C++)

Most sequential containers support:

1. **`init(stl::array<T, N>& storage)`** — elements live in your static array (simplest on MCU).
2. **`init(byte_span storage, capacity)`** — raw byte buffer sized `sizeof(T) * capacity`.
3. **`init_from_arena(Arena& arena, capacity, policy)`** — arena allocates element storage.

Policies (where applicable): `ring_policy::overwrite_on_full`, `vector_policy::growable`, `queue_policy::growable`, etc.

Move-only; no copy. Destructors call `clear()` and release owned storage.

### Status codes (C++)

```cpp
enum class status : std::uint8_t {
    ok, null_ptr, invalid, empty, full, oom, unsupported, not_found
};
```

---

## C API

Include the umbrella header:

```c
#include <memkit.h>   /* pulls memkit_config.h and all container headers */
```

Or include individual headers (`ring.h`, `vector.h`, …). All symbols are C23, `[[nodiscard]]` where supported.

### Tiers

Controlled by `MEMKIT_C_API_FULL` and `MEMKIT_C_API_EXTENDED` in `memkit_config.h`:

**Tier 1 — always available (MCU + MPU)**

| Container | Header | Opaque size macro |
|-----------|--------|-------------------|
| Arena | `arena.h` | (struct fields visible) |
| Ring | `ring.h` | `MEMKIT_RING_OBJ_BYTES` |
| Vector | `vector.h` | `MEMKIT_VECTOR_OBJ_BYTES` |
| Stack | `stack.h` | `MEMKIT_STACK_OBJ_BYTES` |
| Queue | `queue.h` | `MEMKIT_QUEUE_OBJ_BYTES` |
| Bitset | `bitset.h` | `MEMKIT_BITSET_OBJ_BYTES` |
| ObjPool | `objpool.h` | `MEMKIT_OBJPOOL_OBJ_BYTES` |
| HandlePool | `handle_pool.h` | `MEMKIT_HANDLE_POOL_OBJ_BYTES` |

**Tier 2 — full on MPU; MCU stubs link but return `*_ERR_UNSUPPORTED`**

| Container | Header |
|-----------|--------|
| Deque | `deque.h` |
| HashMap | `hashmap.h` |
| BTree | `btree.h` |
| PQueue | `pqueue.h` |
| List | `list.h` |
| DList | `dlist.h` |
| LruCache | `lrucache.h` |

On MCU firmware that needs tier-2 containers, use the C++ API (`memkit.hpp`) with static or arena storage instead of the C stubs.

### Opaque objects

Each container handle embeds implementation storage:

```c
typedef struct ring {
    alignas(max_align_t) unsigned char bytes[MEMKIT_RING_OBJ_BYTES];
} ring_t;
```

Do not read or write `bytes` directly. Sizes are checked in `src/c_api/bindings.cpp` (via `include/memkit/c_api/static_checks.hpp`) at build time.

`MEMKIT_RING_OBJ_BYTES` is **160** (not 128): the ring C API box embeds an `element_callback_bridge` for copy/destroy trampolines. If you change `ring_box` layout, update `memkit_object_sizes.h` and rebuild so the static check catches mismatches.

### Two initialization styles

**1. `*_init` — embed in your struct or stack**

You supply element storage (unless using arena-owned flags):

```c
static uint8_t storage[sizeof(uint32_t) * 4];

ring_t ring;
ring_status_t st = ring_init(&ring, &(ring_config_t){
    .elem_size     = sizeof(uint32_t),
    .capacity      = 4,
    .storage       = storage,
    .storage_bytes = sizeof storage,
});
ring_deinit(&ring);
```

**2. `*_create` — heap/arena allocates container + storage (MPU)**

```c
ring_t *logs = NULL;
ring_create(&logs, sizeof(log_line_t), 32, arena,
            RING_FLAG_OVERWRITE_ON_FULL);
/* … */
ring_destroy(logs);
```

Use `*_FLAG_OWNS_STORAGE`, `*_FLAG_OWNS_SELF`, `*_FLAG_ARENA_STORAGE`, and `*_FLAG_DYNAMIC_STORAGE` to track ownership (see each header).

**Implementation note:** `*_create` helpers allocate the opaque object first, then call `*_init` in place (`include/memkit/c_api/create_object.hpp`). Do not init on a stack temporary and copy the box into arena/heap storage — embedded callback policy pointers must refer to the bridge inside the live object.

### Copy and destroy callbacks

For **POD / trivially copyable** element types, omit `copy_fn` and `destroy_fn` (memkit `memcpy`s).

For non-trivial types, provide:

```c
ring_copy_fn    copy_fn;    /* (void *dst, const void *src, void *user) → status */
ring_destroy_fn destroy_fn; /* (void *elem, void *user) */
void           *user;
```

Hash maps and similar containers add `hash_fn`, `key_eq_fn`, and separate key/value copy/destroy hooks (see `hashmap.h`).

### Status codes (C)

Each container defines `<name>_status_t` and `<name>_status_ok()`. Common values:

| Code | Meaning |
|------|---------|
| `*_OK` | Success |
| `*_ERR_NULL` | Null pointer argument |
| `*_ERR_INVALID` | Bad config or argument |
| `*_ERR_EMPTY` | Pop/peek on empty container |
| `*_ERR_FULL` | Push on full (non-growable, non-overwrite) |
| `*_ERR_OOM` | Allocation failed |
| `*_ERR_UNSUPPORTED` | Tier-2 on MCU, or feature disabled |
| `*_ERR_NOT_FOUND` | Lookup miss (maps, cache) |

Always check return values; do not ignore `[[nodiscard]]` results.

### C example — static storage

```c
#include "ring.h"

static uint8_t ring_storage[sizeof(uint32_t) * 4];

ring_t ring;
ring_init(&ring, &(ring_config_t){
    .elem_size     = sizeof(uint32_t),
    .capacity      = 4,
    .storage       = ring_storage,
    .storage_bytes = sizeof ring_storage,
});

uint32_t value = 42;
ring_push_back(&ring, &value);
ring_deinit(&ring);
```

### Arena (C)

```c
arena_t arena;
arena_init(&arena, &(arena_config_t){
    .backing       = my_buffer,
    .backing_bytes = sizeof my_buffer,
});

void *ptr = NULL;
arena_alloc(&arena, size, alignment, &ptr);
arena_reset(&arena);   /* free all bumps at once */
arena_deinit(&arena);
```

On MPU, `arena_create()` / `arena_create_with_backing()` can allocate mmap or heap backing automatically.

### Ring-specific: DMA-friendly regions

Ring (and queue/deque C APIs built on the same core) expose contiguous read/write windows:

```c
const void *rx = NULL;
size_t n = ring_readable_contiguous(&ring, &rx);
/* process n elements at rx … */
ring_commit_read(&ring, n);

void *tx = NULL;
n = ring_writable_contiguous(&ring, &tx);
/* fill tx … */
ring_commit_write(&ring, n);
```

---

## Container cheat sheet

| Container | C++ class | C type | Tier | Notes |
|-----------|-----------|--------|------|-------|
| Ring | `Ring<T>` | `ring_t` | 1 | Overwrite-on-full policy |
| Queue | `Queue<T>` | `queue_t` | 1 | FIFO |
| Deque | `Deque<T>` | `deque_t` | 2 | Double-ended |
| Vector | `Vector<T>` | `vector_t` | 1 | Growable optional |
| Stack | `Stack<T>` | `stack_t` | 1 | Same core as vector |
| Bitset | `Bitset` | `bitset_t` | 1 | |
| ObjPool | `ObjPool<T>` | `objpool_t` | 1 | Fixed capacity |
| HashMap | `HashMap<K,V>` | `hashmap_t` | 2 | Chaining or open addressing |
| BTree | `BTree<K,V>` | `btree_t` | 2 | Ordered |
| PQueue | `PQueue<T>` | `pqueue_t` | 2 | Heap |
| List | `List<T>` | `list_t` | 2 | Singly linked |
| DList | `DList<T>` | `dlist_t` | 2 | Doubly linked |
| LruCache | `LruCache<K,V>` | `lrucache_t` | 2 | |
| HandlePool | `HandlePool<T>` | `handle_pool_t` | 1 | Stable generation handles |
| SmallString | `SmallString<N>` | — | C++ only | Fixed string, no heap |
| ByteRing | `ByteRing` | — | C++ only | Byte I/O ring (`Ring<uint8_t>` semantics) |
| Arena | `memory::static_arena`, … | `arena_t` | 1 | Bump allocator |

---

## Project layout

```
include/
  memkit.h              C umbrella
  memkit_config.h       Target and feature flags
  memkit_object_sizes.h Opaque C object sizes
  *.h                   Per-container C headers
  memkit/
    memkit.hpp          C++ umbrella (16 containers)
    stl.hpp             Zero-cost STL (MCU); optional heap aliases (MPU)
    containers/         C++ template wrappers
    detail/             Shared cores (internal)
    c_api/              C++ boxes, bridges, lifecycle helpers (internal)
    memory/             Arena, fixed buffer/pool, heap, mmap
src/
  arena.cpp
  mmap_backing.cpp
  c_api/
    bindings.cpp        Single TU for all C API extern "C" bindings
    bindings/*.inc.cpp  Per-container binding fragments (included, not compiled separately)
tests/
  test_*_cpp.cpp        C++ container tests (15)
  test_c_api_smoke.c    C API smoke: tier-1 init + arena create (MCU)
  test_c_api_extended.c Tier-1/2 C API + arena *_create (MPU)
examples/
  example_mcu.cpp       C++ MCU demo
  example_mpu.cpp       C++ MPU demo
  example_mpu.c         C MPU demo (built as example_mpu_c)
```

## Choosing an API

**Prefer C++** when you can use C++26: typed elements, no manual `elem_size` / copy callbacks, all containers on MCU, and clearer ownership via move-only handles.

**Use C** when the translation unit must stay C, or you link memkit into an existing C firmware codebase. On MCU, limit yourself to tier 1 unless you also compile C++ and call `memkit.hpp` for heavier containers.

Both APIs call the same `detail/*_core` implementations; behavior matches when configuration is equivalent.

### STL policy (`memkit/stl.hpp`)

| Build | Available via memkit | Blocked |
|-------|---------------------|---------|
| MCU | `array`, `span`, `optional`, `string_view`, `less`, `hash`, utilities | `memkit::stl::vector`, `memkit::stl::string`; `MEMKIT_USE_STL=1` errors |
| MPU (default) | Same zero-cost types as MCU | Heap aliases unless `MEMKIT_USE_STL=1` |
| MPU + `MEMKIT_USE_STL=1` | Also `vector`, `string` aliases | — |

You may still `#include <vector>` directly in your own MPU code; memkit just does not expose or depend on heap STL on MCU.

## CI

GitHub Actions runs MCU (`make all` + smoke), MPU (`make mpu`), MPU+ASan, CMake/`ctest` (MPU), and macOS builds on push/PR to `main`/`master` (see `.github/workflows/ci.yml`).
