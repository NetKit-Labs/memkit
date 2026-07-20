# Adoption guide

How memkit relates to the C++ standard library, how to choose **MCU vs MPU** builds, how **memory ownership** works, and how to integrate the library **wholesale** or **one container at a time** — a common pattern on embedded projects.

For tutorials see [GETTING_STARTED.md](GETTING_STARTED.md). For design rationale see [DESIGN_PHILOSOPHY.md](DESIGN_PHILOSOPHY.md). For prebuilt bare-metal C archives see [DISTRIBUTING_MCU_C.md](DISTRIBUTING_MCU_C.md).

---

## Standard library surface memkit uses

memkit is **not** STL-free. On **both** MCU and MPU it compiles against a small, fixed set of **standard headers** re-exported as `memkit::stl::*` in [`include/memkit/stl.hpp`](../include/memkit/stl.hpp).

These are **zero-cost** types (header-only at link time on typical toolchains):

| STL facility | `memkit::stl` alias | Role in memkit |
|--------------|---------------------|----------------|
| `std::array` | `array` | Caller-owned static storage backing (`init(storage)`) |
| `std::span` | `span`, `byte_span` | Non-owning views over buffers |
| `std::optional` | `optional` | `try_pop`, `try_peek`, empty states |
| `std::string_view` | `string_view` | Non-owning text (`SmallString::view()`) |
| `std::less`, `std::hash`, … | same names | Comparisons / hashing in maps and heaps |
| `<algorithm>`, `<utility>`, `<type_traits>` | (direct) | `move`, `forward`, traits in cores |
| `<new>` | placement `new` | Construct into **your** buffers only |

**Not used inside memkit cores:** `std::deque`, `std::vector`, `std::list`, `std::map`, `std::unordered_map`, or any other **STL container class**. Those algorithms live in memkit’s own `detail/*_core` implementations.

**Blocked on MCU via memkit:** `memkit::stl::vector` and `memkit::stl::string` (and `MEMKIT_USE_STL=1`). You may still `#include <vector>` in **your own** MPU code; memkit does not depend on heap STL internally.

**MPU optional:** `-DMEMKIT_USE_STL=1` (CMake: `-DMEMKIT_USE_STL=ON`) exposes `memkit::stl::vector` and `memkit::stl::string` aliases only — convenience for application code, not required by memkit containers.

---

## memkit containers vs STL containers

Many memkit types cover the same *jobs* as STL containers, but with **fixed capacity**, **explicit status codes**, **MCU-safe defaults**, and **shared C/C++ cores**.

| memkit (C++ / C) | Closest STL analogue | Important difference |
|------------------|----------------------|----------------------|
| `Ring` / `ring_t` | — | Circular buffer; optional **overwrite-oldest** policy (no STL equivalent) |
| `Queue` / `queue_t` | `std::queue` | Fixed FIFO; fails when full unless growable (MPU) |
| `Deque` / `deque_t` | `std::deque` | Fixed-capacity ring deque; tier-2 C on MPU only |
| `Vector` / `vector_t` | `std::vector` | Bounded or growable; no implicit heap on MCU |
| `Stack` / `cstack_t` | `std::stack` | LIFO over vector core |
| `Bitset` / `bitset_t` | `std::bitset<N>` | Runtime-sized fixed bit set |
| `HashMap` / `hashmap_t` | `std::unordered_map` | Fixed or growable; open addressing or chaining |
| `BTree` / `btree_t` | `std::map` | Fixed-capacity ordered map |
| `FlatMap` | sorted vector + binary search | Small static maps; cache-friendly |
| `PQueue` / `pqueue_t` | `std::priority_queue` | Binary heap in fixed storage |
| `List` / `list_t` | `std::forward_list` | Intrusive-node pool in fixed storage |
| `DList` / `dlist_t` | `std::list` | Doubly linked; fixed node pool |
| `ObjPool` / `objpool_t` | — | Fixed slab alloc/free (not `shared_ptr`) |
| `HandlePool` / `handle_pool_t` | — | Stable ID + generation handles |
| `LruCache` / `lrucache_t` | — | Fixed-capacity LRU |
| `SmallString` | `char[N]` / `std::array<char,N>` | Not `std::string`; no heap |
| `ByteRing` | — | Raw byte I/O + DMA-friendly contiguous views |
| `arena_t` / `static_arena` | — | Bump allocator; not an STL allocator |

---

## memkit-only containers (no STL counterpart)

These are aimed at **embedded** workflows — ISRs, DMA, rate limits, flight recorders, calibration tables — and exist only in the C++ API today:

| Category | Types | Typical use |
|----------|-------|-------------|
| **Lock-free handoff (C++ only)** | `SpscQueue`, `MpscQueue`, `DoubleBuffer` | Wait-free / lock-free ISR ↔ task pipelines — **not on Cortex-M0/M0+** |
| **Intrusive structures** | `IntrusiveListHead`, hooks | Zero-allocation linked lists in your structs |
| **Timing** | `TimerWheel`, `TokenBucket` | Tick scheduling, rate limiting |
| **DMA / signal** | `FixedIoVec` (+ `DoubleBuffer` above) | Scatter/gather I/O |
| **Maps / tables** | `EnumMap`, `LookupTable`, `SparseSet` | Dispatch tables, calibration, active ID sets |
| **Logging / payloads** | `RingLog`, `SmallBuffer`, `FixedVariant` | Flight recorder, binary frames, tagged messages |
| **Bit packing** | `BitReader`, `BitWriter` | Protocol fields |
| **Filtering** | `MovingAverage`, `WindowStats` | Fixed-window statistics |

These handoff types use `std::atomic` internally but **do not** integrate with any RTOS. You must respect producer/consumer roles. Everything else in memkit is **single-context** unless you add your own mutex or critical section.

Full contract, queue vs SPSC/MPSC confusion, and FreeRTOS examples: **[CONCURRENCY.md](CONCURRENCY.md)**.

Full picker: [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md).

---

## Build targets: MCU vs MPU

Authoritative flags: [`include/memkit_config.h`](../include/memkit_config.h).

| | **MCU** (bare-metal / RTOS firmware) | **MPU** (embedded Linux, macOS, Windows host) |
|--|--------------------------------------|-----------------------------------------------|
| **Define** | `MEMKIT_MCU=1` | `MEMKIT_MPU=1` (+ `EMBEDDED_LINUX=1` on Linux; optional on Windows/macOS) |
| **Heap inside memkit** | off (`MEMKIT_ALLOW_HEAP=0`) | on (`MEMKIT_ALLOW_HEAP=1`) |
| **mmap arenas** | off | on (`MEMKIT_ALLOW_MMAP=1`, can disable) |
| **Default arena backing** | caller fixed buffer | mmap |
| **C API tier 2** | stubs → `*_ERR_UNSUPPORTED` | full |
| **C++ containers** | all 32 utilities | all 32 utilities |
| **Heap STL via memkit** | compile error if `MEMKIT_USE_STL=1` | off by default; opt-in with `MEMKIT_USE_STL=1` |

### Makefile

```bash
make all              # MCU — default CXXFLAGS/CFLAGS in Makefile
make mpu              # MPU — adds heap + mmap + tier-2 C tests
make lib-mcu-c        # Freestanding tier-1 C archive (see below)
```

### Manual compile flags

**C++ (MCU or MPU):** add **`-fno-exceptions -fno-rtti`** when compiling against memkit headers (Makefile and CMake set these automatically).

**MCU (C or C++):**

```text
-DMEMKIT_MCU=1 -I/path/to/memkit/include
```

For C++ firmware also:

```text
-fno-exceptions -fno-rtti
```

**MPU (Linux embedded):**

```text
-DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1 -I/path/to/memkit/include
```

**MPU (Windows / macOS host):**

```text
-DMEMKIT_MPU=1 -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1 -I/path/to/memkit/include
```

**MPU + optional heap STL aliases** (application convenience only):

```text
-DMEMKIT_USE_STL=1
```

**CMake:** `-DMEMKIT_EMBEDDED_LINUX=ON` for MPU on embedded Linux; `-DMEMKIT_MPU=ON` for MPU on any host (including Windows VirtualAlloc backing); `-DMEMKIT_USE_STL=ON` for heap STL aliases.

---

## Memory ownership: caller vs callee

Containers never touch the heap on MCU unless you explicitly use MPU-only `*_create` / growable paths. Ownership is always explicit.

### C++ patterns

| Pattern | Who owns bytes | API |
|---------|----------------|-----|
| Static / stack buffer | **Caller** | `init(stl::array<T,N>&)` or `init(byte_span, capacity)` |
| Arena-backed | **Caller** owns arena buffer; memkit bumps inside it | `init_from_arena(arena, capacity, policy)` |
| Growable (MPU) | **Callee** may reallocate via heap if no arena | growable policies + `init_from_arena` or heap path |

Move-only handles; destructors call `clear()`. No copy.

### C patterns

| Pattern | C API | Pairing |
|---------|-------|---------|
| Embed handle in your struct | `*_init` + `*_deinit` | Caller supplies `storage` in `*_config_t` |
| Library allocates (MPU) | `*_create` + `*_destroy` | Often with `arena_t*` or heap |

**Ownership flags** (see each `*.h`): `*_FLAG_OWNS_STORAGE`, `*_FLAG_OWNS_SELF`, `*_FLAG_ARENA_STORAGE`, `*_FLAG_DYNAMIC_STORAGE`.

**Rule:** never `*_init` on a temporary and copy the opaque blob — callback bridges inside the live object must stay at a stable address (see README C API note on `create_object.hpp`).

### C opaque layout

```c
typedef struct ring {
    alignas(max_align_t) unsigned char bytes[MEMKIT_RING_OBJ_BYTES];
} ring_t;
```

Sizes are checked at library build time; do not read or write `bytes` directly.

---

## Integration mode 1 — full library

Best when you want CMake/FetchContent or the stock Makefile and multiple containers.

```bash
cmake -B build                           # MCU
cmake -B build -DMEMKIT_EMBEDDED_LINUX=ON   # MPU
cmake --build build
```

Link the static library built from `src/arena.cpp`, `src/mmap_backing.cpp`, and `src/c_api/bindings.cpp` (MPU), or use the `memkit` CMake target.

C++: `#include <memkit/memkit.hpp>`  
C: `#include <memkit.h>`

See [GETTING_STARTED.md](GETTING_STARTED.md).

---

## Integration mode 2 — prebuilt C archive (pure C firmware)

For **C-only** projects that link with `cc` and must **not** pull libstdc++:

```bash
make lib-mcu-c              # → build/libmemkit_mcu_c.a
make check-lib-mcu-c
make test-lib-mcu-c-link
```

Ship `libmemkit_mcu_c.a` + `include/*.h` (built with `-DMEMKIT_MCU=1`, `-fno-exceptions`, `-fno-rtti`, `-ffreestanding`).

**Customer link:**

```bash
arm-none-eabi-gcc -std=c23 -I.../include -DMEMKIT_MCU=1 -c app.c
arm-none-eabi-gcc -o firmware.elf app.o -L.../lib -lmemkit_mcu_c
# libc only — NO -lstdc++
```

This archive contains **tier-1 C API** (arena + all tier-1 bindings in one TU). You cannot strip it to a single symbol at link time without rebuilding from source.

Full details: [DISTRIBUTING_MCU_C.md](DISTRIBUTING_MCU_C.md).

---

## Integration mode 3 — piecemeal C++ (vendoring headers)

Common on embedded teams: copy only what you need into a vendor tree (`third_party/memkit/…`) and add `-I` to your existing firmware build.

### When this works well

- **C++ firmware** with a C++26-capable toolchain
- **One or a few containers** with **caller-owned static storage**
- No need for the C API or `src/*.cpp` for that container

Many sequential containers (`Ring`, `Queue`, `Vector`, `Stack`, `Bitset`, …) are **header-only** through their `detail/*_core` — no memkit `.o` required if you use `init(storage)` only.

### Example: `Ring` only (static storage, MCU)

**1. Copy these files** (paths under repo `include/`):

```text
memkit_config.h
memkit/containers/ring.hpp
memkit/config.hpp
memkit/status.hpp
memkit/stl.hpp
memkit/detail/ring_core.hpp
memkit/detail/ring_buffer_core.hpp
memkit/detail/element_policy.hpp
memkit/detail/element_ops.hpp
memkit/detail/storage_view.hpp
memkit/detail/growable_storage.hpp   # included by ring core; MCU growable paths are disabled
```

**2. Compile your firmware** (adjust toolchain):

```bash
arm-none-eabi-g++ -std=c++26 -Ithird_party/memkit/include -DMEMKIT_MCU=1 \
  -fno-exceptions -fno-rtti -c app.cpp
```

**3. Use in application code:**

```cpp
#include <memkit/containers/ring.hpp>

memkit::stl::array<int, 8> storage{};
memkit::Ring<int> ring;
ring.init(storage);
ring.push_back(42);
```

No `libmemkit.a` required for this path.

### Adding arena-backed init

If you call `init_from_arena`, also vendor:

```text
memkit/memory/fixed_buffer.hpp
memkit/memory/arena.hpp
memkit/detail/utility.hpp
```

You still do **not** need `src/arena.cpp` for C++ `memory::static_arena` — that class is header-only. The **C** `arena_t` API lives in `src/arena.cpp` and is only needed when linking the C API.

### Piecemeal C++ checklist

| Need | Vendor headers | Link memkit `.a`? | Compile flags |
|------|----------------|-------------------|---------------|
| `Ring` / `Queue` with static `init` | container + `detail/*` chain | No | `-fno-exceptions -fno-rtti` |
| `SpscQueue` / `MpscQueue` | container + core + `std::atomic` | No (may need `-latomic`) | `-fno-exceptions -fno-rtti` |
| C API `ring_t` from `.c` files | public `*.h` | **Yes** — `libmemkit_mcu_c.a` or full `lib` | C only; archive built with `-fno-exceptions -fno-rtti` |
| `*_create` / heap growable (MPU) | headers + `memory/heap.hpp` | Often yes (`arena.cpp`, bindings) | `-fno-exceptions -fno-rtti` |
| Tier-2 C (`hashmap_t`, …) | headers | **Yes** — MPU build of library | `-fno-exceptions -fno-rtti` |

### Why the C API is not per-file

All `extern "C"` bindings compile in **one** translation unit ([`src/c_api/bindings.cpp`](../src/c_api/bindings.cpp)) for size and consistency. Piecemeal **C** integration realistically means:

- **Tier-1:** prebuilt `libmemkit_mcu_c.a`, or  
- **C++ header vendoring** from `.cpp` translation units, calling templates directly.

---

## Integration mode 4 — piecemeal C with one container

**Realistic approach for pure C:**

1. Build `libmemkit_mcu_c.a` once for your target ABI (`make lib-mcu-c` with your cross `CXX`).
2. Ship the archive + only the headers you document to your team (`ring.h`, `memkit_helpers.h`, `memkit_config.h`, …).
3. Include only what you use in application code; the linker drops unused objects if you use `--gc-sections` / `-dead_strip` (container code may still share core symbols inside the archive).

**Minimal C ring example** (static storage, no heap):

```c
#include <ring.h>
#include <memkit_helpers.h>

MEMKIT_ELEM_STORAGE(int, 8, buf);

int main(void)
{
    ring_t ring;
    if (!ring_status_ok(MEMKIT_RING_INIT_STATIC(&ring, int, buf))) {
        return 1;
    }
    int v = 42;
    ring_push_back(&ring, &v);
    ring_deinit(&ring);
    return 0;
}
```

You do **not** need `arena.c` in your project — arena support is already inside `libmemkit_mcu_c.a`. You only call arena APIs if your design uses `ring_create(..., arena, ...)`.

---

## Quick decision tree

```text
Pure C firmware, smallest link?
  → libmemkit_mcu_c.a + tier-1 headers (mode 2)

C++ firmware, one container, static storage?
  → Vendor headers (mode 3)

Need hashmap/deque/… in C on Linux?
  → Full MPU library, MEMKIT_MPU=1 (mode 1)

Need every container on MCU in C?
  → Not tier-1 C — use C++ headers (mode 3) or MPU build
```

---

## Related docs

| Doc | Contents |
|-----|----------|
| [CONCURRENCY.md](CONCURRENCY.md) | Thread/ISR contract, lock-free trio, FreeRTOS patterns |
| [GETTING_STARTED.md](GETTING_STARTED.md) | First steps, C/C++ examples, CMake FetchContent |
| [C_API_REFERENCE.md](C_API_REFERENCE.md) | C config fields, flags, function parameters |
| [CXX_API_REFERENCE.md](CXX_API_REFERENCE.md) | C++ init overloads, policies, methods |
| [DESIGN_PHILOSOPHY.md](DESIGN_PHILOSOPHY.md) | Why MCU/MPU, memory models, STL policy |
| [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | Which container for which job |
| [DISTRIBUTING_MCU_C.md](DISTRIBUTING_MCU_C.md) | Prebuilt `.a`, freestanding flags, verification |
| [README](../README.md) | Full API reference |
