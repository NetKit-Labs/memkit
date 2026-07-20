# memkit

Embedded-friendly containers for C and C++ with a single shared implementation per container family. C++ templates are the primary API; the C API is a thin, type-erased layer over the same cores.

**New here?** [Getting started](docs/GETTING_STARTED.md) · [Adoption guide](docs/ADOPTION_GUIDE.md) · [Concurrency & RTOS](docs/CONCURRENCY.md) · [C API reference](docs/C_API_REFERENCE.md) · [C++ API reference](docs/CXX_API_REFERENCE.md) · [Design philosophy](docs/DESIGN_PHILOSOPHY.md) · [Which container?](docs/CONTAINER_GUIDE.md) · [Bare-metal C lib](docs/DISTRIBUTING_MCU_C.md)

## Quick start

```bash
make all              # lib + 32 C++ tests + 8 C API tests + 4 MCU examples
make benchmark        # timing + size vs hand-rolled C
make test_c_api       # tier-1 C API tests (MCU)
make lib-mcu-c        # freestanding libmemkit_mcu_c.a (C customers, no libstdc++)
make mpu              # MPU: examples + tier-2 C API tests + integration test
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

**C — static ring on MCU** ([`memkit_helpers.h`](include/memkit_helpers.h) macros included via `memkit.h`):

```c
#include <memkit.h>

MEMKIT_ELEM_STORAGE(int, 8, storage);

ring_t ring;
MEMKIT_RING_INIT_STATIC(&ring, int, storage);

int value = 42;
ring_push_back(&ring, &value);
ring_deinit(&ring);
```

See [Build](#build), [Design philosophy](docs/DESIGN_PHILOSOPHY.md), [C++ API](#c-api), and [C API](#c-api-1) below for full details.

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
- **Zero-overhead & OS-agnostic.** No internal mutexes, no virtual functions, no RTTI. Polymorphism is compile-time (templates + policy structs). Maximum hot-path speed and minimum flash footprint on bare-metal targets.

**C++ (32 utilities)** — templates and header-only classes in `include/memkit/containers/`, included via `<memkit/memkit.hpp>`.

**C (14 containers + arena)** — type-erased by design (C23 has no generics). Each container exposes `*_init` / `*_create` / `*_destroy` over a shared `*_box` implementation. Utility types (`SmallString`, `SmallBuffer`, queues, maps, `FixedVariant`, `TokenBucket`, `FixedIoVec`, `LookupTable`, etc.) are **C++-only** by design.

## Zero-overhead & OS-agnostic architecture

memkit is architected for **embedded systems that cannot afford hidden synchronization costs**. The library ships **no internal mutexes**, **no RTOS bindings**, and **no virtual dispatch** — deliberate choices that keep execution predictable, images small, and the library portable across bare-metal, RTOS, and embedded-Linux targets.

Containers fall into **two strategic categories**:

| Category | Types | Concurrency model | Primary advantage |
|----------|-------|-------------------|-------------------|
| **Lock-free utilities** | `SpscQueue`, `MpscQueue`, `DoubleBuffer` (C++ only) | Fixed producer/consumer roles; hardware atomics | Wait-free or lock-free handoff across tasks and ISRs |
| **Single-threaded containers** | `HashMap`, `Vector`, `BTree`, `FlatMap`, `EnumMap`, `Ring`, `Queue`, C API (`ring_t`, `queue_t`, …), etc. | One owning context by default | Raw, ultra-fast paths with zero locking overhead |

Full integration guide (FreeRTOS patterns, queue naming, `-latomic`): **[docs/CONCURRENCY.md](docs/CONCURRENCY.md)**.

### Category 1 — Lock-free trio (C++ only)

Three headers implement **high-performance communication pipelines** — safely moving data between RTOS tasks, or between an **Interrupt Service Routine (ISR)** and the main background loop, without OS-provided queues or library-internal locks.

| Type | Header | Pattern | Progress guarantee |
|------|--------|---------|-------------------|
| **`SpscQueue<T>`** | [`spsc_queue.hpp`](include/memkit/containers/spsc_queue.hpp) | Single-producer, single-consumer FIFO | **Wait-free** (bounded steps on the hot path when roles are respected) |
| **`MpscQueue<T>`** | [`mpsc_queue.hpp`](include/memkit/containers/mpsc_queue.hpp) | Multi-producer, **single**-consumer FIFO | **Lock-free** (Vyukov-style; producers may spin briefly) |
| **`DoubleBuffer<T>`** | [`double_buffer.hpp`](include/memkit/containers/double_buffer.hpp) | Ping-pong buffer with `write_span()` → `publish()` → `read_span()` | **Wait-free** handoff of a full data block |

These are **not** general-purpose “thread-safe containers.” Each type defines **who may produce and who may consume**. Breaking those roles requires application-level synchronization.

> **Naming note:** C++ `Queue` / C `queue_t` are **single-threaded FIFO rings**, not substitutes for `SpscQueue` or `MpscQueue`.

### Category 2 — Single-threaded containers (bare-metal optimized)

The remaining library — including **`HashMap`**, **`Vector`**, **`BTree`**, **`FlatMap`**, **`EnumMap`**, **`Ring`**, **`Queue`**, pools, lists, the **C API**, and **`arena`** — is **strictly single-threaded** by design: no hidden locking, no atomic overhead, direct memory access on the hot path.

**RTOS integration:** when a container must be shared across multiple tasks, wrap it at the **application layer** with your OS-native primitive (mutex, semaphore, or critical section). memkit stays OS-agnostic; you choose the lock that matches your latency and context rules.

```cpp
#include <memkit/memkit.hpp>
// Your RTOS headers, e.g. #include "FreeRTOS.h" / #include "semphr.h"

template<typename Fn>
memkit::status with_container_lock(SemaphoreHandle_t mu, Fn&& fn)
{
    if (xSemaphoreTake(mu, portMAX_DELAY) != pdTRUE) {
        return memkit::status::invalid;
    }
    const memkit::status st = fn();
    xSemaphoreGive(mu);
    return st;
}

// Example: two tasks sharing one memkit::Queue
static memkit::Queue<sample_t> g_samples;
static SemaphoreHandle_t       g_samples_mu;

memkit::status push_sample(const sample_t& s)
{
    return with_container_lock(g_samples_mu, [&] { return g_samples.push_back(s); });
}
```

Pure **C** firmware uses the same pattern around `queue_t`, `ring_t`, etc. Tier-1 C has **no** lock-free queue; use C++ for the trio, an RTOS queue for ISR handoff, or single-task ownership.

### Hardware & ISA compatibility (lock-free trio)

Lock-free utilities depend on `std::atomic` lowering to **native hardware synchronization** (exclusive load/store or equivalent). **Single-threaded containers run on any target** memkit supports; the table below applies **only** to `SpscQueue`, `MpscQueue`, and `DoubleBuffer`.

| Platform | Lock-free trio | Mechanism / notes |
|----------|:--------------:|-------------------|
| **ARM Cortex-M0 / M0+** (ARMv6-M) | **Not supported** | No `LDREX`/`STREX` (or equivalent). Use single-threaded containers or an RTOS queue for cross-context data. |
| **ARM Cortex-M3 / M4 / M4F / M7** (ARMv7-M) | **Fully supported** | Native `LDREX`/`STREX` — true lock-free execution on the hot path. |
| **ARM Cortex-M33** (ARMv8-M) | **Highly optimized** | `LDAEX`/`STLEX` with integrated barriers — hardware-enforced ordering for maximum efficiency. |
| **Desktop / server** (x86, x64, ARM A-series) | **Fully supported** | Native atomic instructions; also used for host CI and MPU builds. |

On supported embedded GCC toolchains, linking the lock-free trio may require **`-latomic`**. See [DISTRIBUTING_MCU_C.md](docs/DISTRIBUTING_MCU_C.md).

### API completeness (v0.2)

The public API is **feature-complete** for embedded use on **MCU and MPU hosts** (Linux, macOS, and Windows). Every shipped container is listed in the [C++ API reference](#c-api) and [C API reference](#c-api-1) below; authoritative signatures live in the headers.

| Surface | Count | MCU | MPU | Notes |
|---------|-------|-----|-----|-------|
| C++ utilities | 32 | all | all | `#include <memkit/memkit.hpp>` |
| C containers | 14 | tier 1 (8) | tier 1 + tier 2 (14) | `#include <memkit.h>` or per-container `*.h` |
| C arena | 1 | yes | yes | Bump allocator; heap/mmap create on MPU |
| C++-only helpers | 18 | yes | yes | No C bindings — see [cheat sheet](#container-cheat-sheet) |

**C/C++ parity:** the **14 C-bound containers** and **`arena_t`** call the same `detail/*_core` implementations as their C++ counterparts — behavior matches when configuration is equivalent (storage, flags, callbacks). **18 utilities** (lock-free trio, `ByteRing`, `SmallString`, …) are **C++-only** by design: templates, typed elements, or hardware atomics without a practical C surface. On MCU, tier-2 C headers link but return `*_ERR_UNSUPPORTED`; use C++ headers for those containers on firmware.

**Tests:** 32 C++ test binaries (including `test_arena_cpp` for arena overflow checks) cover all 32 C++ containers (`Stack` and `Queue` share `test_stack_queue_cpp.cpp`). C API: **8 tier-1 tests** on MCU (`test_arena_c`, `test_ring_c`, … — one per header), **7 tier-2 tests** on MPU, plus **`test_c_api_extended.c`** (shared `arena_create` + all `*_create` helpers on one arena). MPU also runs **`test_heap_arena_cpp`** (`heap_arena` + arena-backed ring).

Pick the API that fits your project:

| Use case | API |
|----------|-----|
| Firmware (C), small image, tier-1 containers | C API tier 1 |
| Firmware (C++), all containers, static/arena storage | `#include <memkit/memkit.hpp>` |
| Embedded Linux (C or C++), heap/mmap | Either API; tier 2 available in C |
| Mixed C/C++ codebase | C API from C; C++ templates from C++ |

## Targets

For **why** MCU and MPU exist and how to choose memory models, see [Design philosophy](docs/DESIGN_PHILOSOPHY.md). Quick reference:

memkit distinguishes two build targets (see `include/memkit_config.h`):

| | **MCU** (bare-metal) | **MPU** (embedded Linux, macOS, Windows host) |
|--|----------------------|-----------------------------------------------|
| Default define | `MEMKIT_MCU=1` | `MEMKIT_MPU=1` (+ `EMBEDDED_LINUX=1` on Linux; optional elsewhere) |
| Heap (`malloc`) | off | on (`MEMKIT_ALLOW_HEAP=1`) |
| Virtual arena backing | off | on (`MEMKIT_ALLOW_MMAP=1`) — POSIX `mmap` or Windows `VirtualAlloc` |
| Default arena backing | caller buffer | virtual memory (when enabled) |
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
| Fixed pool | slab / pool storage | `ObjPool<T>` / `objpool_t` | yes | yes |
| Arena | `arena_t *` + `*_FLAG_ARENA_STORAGE` | `init_from_arena(arena, …)` | yes | yes |
| Heap | `*_FLAG_DYNAMIC_STORAGE` (create helpers) | heap arena / growable | no | yes |
| mmap / VirtualAlloc | arena with virtual backing | `memory::mmap_arena` | no | yes (Linux/macOS: `mmap`; Windows: `VirtualAlloc`) |

**Arena** is a bump allocator: fast sequential allocation, reset the whole arena in one call. Good for short-lived batches of container storage.

**Growable** (vector, stack, queue, deque, pqueue, hashmap): doubles capacity when full; on MPU may use heap if no arena is supplied.

## Build

All C++ targets (library, tests, examples, benchmarks) compile with **`-fno-exceptions`** and **`-fno-rtti`**. memkit uses status codes, not exceptions; the flags match embedded firmware practice and are enforced in the Makefile and CMake.

The default Makefile target builds the MCU library and C++ tests:

```bash
make all              # lib + 32 C++ tests + 8 C API tests + 4 MCU examples
make check-lib        # verify library objects have no exception/RTTI symbols
make benchmark        # timing + size vs hand-rolled C
make test_cpp         # C++ container tests only
make test_c_api       # tier-1 C API tests (MCU, 8 binaries)
make mpu              # MPU: tier-2 C API tests + test_heap_arena_cpp + integration test
make lib-mcu-c        # freestanding tier-1 C archive for pure-C firmware
make clean
```

### CMake

```bash
cmake -B build                           # MCU (default)
cmake -B build -DMEMKIT_EMBEDDED_LINUX=ON   # MPU on embedded Linux
cmake -B build -DMEMKIT_MPU=ON              # MPU (Linux, macOS, or Windows)
cmake --build build
ctest --test-dir build
```

On **Windows MPU**, `MEMKIT_MPU=ON` enables arena virtual backing via `VirtualAlloc` (no `EMBEDDED_LINUX` required). CI builds Windows MPU with LLVM Clang; CMake also sets `/GR-` and `/EHsc-` when using MSVC.

MPU builds also produce `example_mpu` (C++) and `example_mpu_c` (C).

Link against the static library built from `src/arena.cpp`, `src/mmap_backing.cpp`, and `src/c_api/bindings.cpp`. Add `-Iinclude`, `-DMEMKIT_MCU=1` (MCU) or `-DMEMKIT_MPU=1` with `-DEMBEDDED_LINUX=1` on embedded Linux (Windows/macOS MPU: `-DMEMKIT_MPU=1` alone is enough), and **`-fno-exceptions -fno-rtti`** when compiling C++ against memkit headers.

---

## C++ API

Include the umbrella header:

```cpp
#include <memkit/memkit.hpp>
```

**Parameter reference:** [docs/CXX_API_REFERENCE.md](docs/CXX_API_REFERENCE.md) — init overloads, policies, return types. Authoritative signatures: headers in `include/memkit/`.

All containers live in namespace `memkit`. Operations return `memkit::status`; use `memkit::ok(st)` to test success.

### Containers (complete)

All types live in namespace `memkit`. Operations return `memkit::status` unless noted. Every class supports `init` from caller storage; most also support `init_from_arena`. Move-only; destructors call `clear()`.

| Class | Header | Role | Key API |
|-------|--------|------|---------|
| `Ring<T>` | `ring.hpp` | Circular buffer | `init`, `init_from_arena`, `push_back`/`front`, `pop_*`, `try_*`, `readable_contiguous`, `writable_contiguous`, `commit_read`/`write`, `ring_policy` |
| `Queue<T>` | `queue.hpp` | FIFO ring | `init`, `init_from_arena`, `push_back`, `pop_front`, `peek_*`, `queue_policy` |
| `Deque<T>` | `deque.hpp` | Double-ended ring | `init`, `init_from_arena`, `push_back`/`front`, `pop_*`, `peek_*`, `deque_policy` |
| `Vector<T>` | `vector.hpp` | Contiguous array | `init`, `init_from_arena`, `push_back`, `pop_back`, `peek_at`, `set_at`, `at`, `reserve`, `vector_policy` |
| `Stack<T>` | `stack.hpp` | LIFO (vector core) | `init`, `init_from_arena`, `push`, `pop`, `peek`, `stack_policy` |
| `Bitset` | `bitset.hpp` | Fixed bit set | `init`, `init_from_arena`, `set`/`reset`/`test`/`toggle`, `find_first_*`, `union_with`, `intersect_with`, `load_bytes`/`store_bytes` |
| `ObjPool<T>` | `objpool.hpp` | Object pool | `init`, `init_from_arena`, `alloc`, `free`, `contains` |
| `HashMap<K,V>` | `hashmap.hpp` | Hash map | `init`, `init_from_arena`, `put`, `get`, `remove`, `contains`, `foreach`, `hashmap_strategy`, `hashmap_policy` |
| `BTree<K,V>` | `btree.hpp` | Ordered map | `init`, `init_from_arena`, `insert`, `get`, `remove`, `contains`, `peek_min`/`max`, `foreach` |
| `PQueue<T,Compare>` | `pqueue.hpp` | Binary heap | `init`, `init_from_arena`, `push`, `pop`, `peek`, `pqueue_policy` |
| `List<T>` | `list.hpp` | Singly linked list | `init`, `init_from_arena`, `push_*`, `pop_*`, `insert_at`, `remove_at`, `foreach` |
| `DList<T>` | `dlist.hpp` | Doubly linked list | Same as `List` plus `pop_back`, `foreach_reverse`, `front`/`back` pointers |
| `LruCache<K,V>` | `lrucache.hpp` | LRU cache | `init`, `init_from_arena`, `get`, `put`, `remove`, `touch`, `contains`, `foreach_mru`/`lru` |
| `HandlePool<T>` | `handle_pool.hpp` | Stable handles | `init`, `init_from_arena`, `acquire`, `release`, `valid`, `get`, `handle_t` |
| `SmallString<N>` | `small_string.hpp` | Fixed string | `assign`, `append`, `clear`, `size`, `empty`, `view`, `c_str`, `operator==` |
| `ByteRing` | `byte_ring.hpp` | Byte I/O ring | `init`, `init_from_arena`, `push_bytes`, `readable_contiguous`, `writable_contiguous`, `commit_read`/`write` |
| `IntrusiveListHead` | `intrusive_list.hpp` | Intrusive lists | `push_back`/`front`, `erase`, `splice`, `is_linked`; hooks: `IntrusiveListHook`, `IntrusiveDListHook` |
| `SpscQueue<T>` | `spsc_queue.hpp` | Wait-free SPSC handoff | `#include <memkit/containers/spsc_queue.hpp>` — not `Queue` |
| `FlatMap<K,V>` | `flat_map.hpp` | Sorted flat map | `init`, `init_from_arena`, `put`, `get`, `remove`, `contains`, `find`, `foreach` |
| `TimerWheel<N>` | `timer_wheel.hpp` | Timing wheel | `init`, `schedule`, `cancel`, `tick`; nodes: `TimerWheelNode` |
| `DoubleBuffer<T>` | `double_buffer.hpp` | Wait-free ping-pong block | `write_span`, `publish`, `read_span` |
| `MpscQueue<T>` | `mpsc_queue.hpp` | Lock-free MPSC handoff | `#include <memkit/containers/mpsc_queue.hpp>` — one consumer only |
| `EnumMap<Enum,V,N>` | `enum_map.hpp` | Enum map | `init`, `put`, `get`, `at`, `contains`, `clear`, `foreach` |
| `RingLog<Record>` | `ring_log.hpp` | Flight recorder | `init`, `init_from_arena`, `append`, `clear`, `size`, `capacity`, `foreach` |
| `SparseSet` | `sparse_set.hpp` | Active ID set | `init`, `init_from_arena`, `insert`, `remove`, `contains`, `clear`, dense `operator[]` |
| `SmallBuffer<N>` | `small_buffer.hpp` | Binary payload | `assign`, `clear`, `size`, `capacity`, `empty`, `data`, `span` |
| `FixedVariant<Ts...>` | `fixed_variant.hpp` | Tagged union | `emplace`, `holds`, `get`, `destroy`, `index` |
| `TokenBucket` | `token_bucket.hpp` | Rate limiter | `init`, `try_consume`, `consume`, `refill`, `reset`, `tokens` |
| `FixedIoVec<N>` | `fixed_iovec.hpp` | Scatter/gather | `push`, `clear`, `size`, `empty`, `slice`, `span` |
| `LookupTable<X,Y>` | `lookup_table.hpp` | Calibration table | `init`, `at`, `lookup` (interpolate), `size` |
| `BitReader` / `BitWriter` | `bit_stream.hpp` | Packed bits | `read`/`write`, `read_bits`/`write_bits`, `flush`, `reset` |
| `MovingAverage<T,N>` | `running_stats.hpp` | Moving average | `push`, `average`, `clear`, `empty`, `full` |
| `WindowStats<T,N>` | `running_stats.hpp` | Window stats | `push`, `min`, `max`, `average`, `clear` |

**Memory helpers** (`memkit/memory/`): `fixed_buffer`, `static_arena`; on MPU also `heap_arena`, `mmap_arena`, `mmap_storage`, `heap_storage`.

Type alias: `memkit::Arena<…>`.

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

### Intrusive lists, SPSC queue, FlatMap, and TimerWheel (embedded utilities)

```cpp
// Intrusive list — embed hooks in your structs, no node allocation
struct work_item {
    memkit::IntrusiveListHook hook{};
    int id = 0;
};
memkit::IntrusiveListHead pending;
pending.push_back(item.hook);

// SPSC queue — one producer, one consumer (e.g. ISR push, task pop). Not the same as Queue.
memkit::stl::array<std::byte, memkit::SpscQueue<int>::storage_bytes(16)> qbuf{};
memkit::SpscQueue<int> events;
events.init(qbuf, 16u);
events.push(42);  // producer side only
int v; events.pop(v);  // consumer side only

// FlatMap — sorted array for small static maps (O(log n) lookup)
memkit::stl::array<std::byte, memkit::FlatMap<int,int>::storage_bytes(8)> mapbuf{};
memkit::FlatMap<int,int> cfg;
cfg.init(mapbuf, 8u);
cfg.put(1, 100);

// TimerWheel — schedule callbacks N ticks in the future (intrusive nodes)
memkit::TimerWheel<64> timers;
timers.init();
memkit::TimerWheelNode node{ .callback = my_cb, .user = ctx };
timers.schedule(node, 10u);
timers.tick();  // advance one tick
```

### DoubleBuffer, MpscQueue, EnumMap, RingLog, and SparseSet

```cpp
// Ping-pong DMA buffer
memkit::DoubleBuffer<std::uint16_t> adc;
adc.init(backing);  // 2 x slot_capacity samples
adc.write_span()[0] = sample;
adc.publish();

// Multi-ISR → main loop queue
memkit::MpscQueue<event_t> events;
events.init(qbuf, 16u);

// Mode / dispatch table keyed by enum
memkit::EnumMap<mode, handler_t, 4> dispatch;

// Crash/debug flight recorder
memkit::RingLog<log_record> flight;
flight.init(logbuf, 64u);
flight.append({tick, code});

// Track active entity/timer IDs with fast iteration
memkit::SparseSet active;
active.init(dense, sparse, max_entities);
active.insert(id);
for (std::size_t i = 0; i < active.size(); ++i) { use(active[i]); }
```

### SmallBuffer, FixedVariant, TokenBucket, FixedIoVec, and LookupTable

```cpp
// Protocol payload (binary, not null-terminated)
memkit::SmallBuffer<64> frame;
frame.assign(payload, len);

// Message dispatch without heap
memkit::FixedVariant<cmd_a, cmd_b, cmd_c> msg;
msg.emplace<cmd_b>(cmd_b{ .id = 3 });

// UART / CAN rate limiting
memkit::TokenBucket tx_limit;
tx_limit.init(100u, 5u);  // capacity, refill per tick
tx_limit.refill();
if (memkit::ok(tx_limit.try_consume())) { send_byte(); }

// DMA scatter/gather
memkit::FixedIoVec<4> tx;
tx.push(header, sizeof header);
tx.push(payload, payload_len);

// Sensor calibration
const std::int32_t adc_keys[] = {0, 1023};
const float volts[] = {0.0f, 3.3f};
memkit::LookupTable<std::int32_t, float> curve;
curve.init(adc_keys, volts, 2u);
float v = curve.at(raw_adc);
```

---

## Composition patterns

Most firmware workflows combine a few memkit types rather than needing new containers. Common recipes:

| Pattern | Building blocks | Notes |
|---------|-----------------|-------|
| **ISR → main handoff** | `SpscQueue<T>` (one ISR) or `MpscQueue<T>` (several ISRs) | Wait-free / lock-free; fixed roles — [architecture](#zero-overhead--os-agnostic-architecture) |
| **Deferred work / tasks** | `IntrusiveListHead` + `TimerWheel<N>` | Embed hooks in your structs; schedule callbacks |
| **Command dispatch** | `EnumMap<cmd, handler>` or `FlatMap<id, fn>` | O(1) enum table for small sets |
| **Active entity / timer set** | `SparseSet` or `HandlePool<T>` + `Bitset` | Dense iteration over live IDs |
| **Protocol payload** | `SmallBuffer<N>` + `BitReader` / `BitWriter` | Length-prefixed binary + packed fields |
| **DMA / ADC pipeline** | `DoubleBuffer<T>` or `FixedIoVec<N>` | Wait-free block handoff via `publish()`; scatter/gather for I/O |
| **Sensor filtering** | `MovingAverage<T,N>` or `WindowStats<T,N>` | Fixed window; no heap |
| **Calibration** | `LookupTable<X,Y>` over flash arrays | Interpolate ADC → engineering units |
| **Rate-limited I/O** | `TokenBucket` + `ByteRing` / UART driver | Call `refill()` from tick, `try_consume()` before TX |
| **Flight recorder** | `RingLog<Record>` | Overwrites oldest; dump newest-first after fault |
| **Typed messages** | `FixedVariant<Ts...>` | ISR/main queue of discriminated message types |

### Example: multi-ISR event queue + calibration

See `examples/example_embedded_patterns.cpp` — DMA ping-pong, `MpscQueue`, `LookupTable`, bit-stream decode, and moving average in one MCU-runnable demo.

```cpp
// Multiple ISRs push; main loop pops
alignas(std::max_align_t) memkit::stl::array<std::byte, N> qbuf{};
memkit::MpscQueue<event_t> events;
events.init(qbuf.data(), 16u);

void uart_isr() { (void)events.push(event_t{ .src = UART, .code = byte }); }
void main_loop() {
    event_t e;
    while (memkit::ok(events.pop(e))) { dispatch(e); }
}

// Flash-resident calibration
static const std::int32_t adc_x[] = {0, 1023};
static const float adc_y[] = {0.0f, 3.3f};
memkit::LookupTable<std::int32_t, float> curve;
curve.init(adc_x, adc_y, 2u);
float volts = curve.at(raw_adc);
```

### Example: DMA ping-pong

```cpp
memkit::DoubleBuffer<adc_block> dma;
dma.init(backing.data(), 1u);

// ISR / DMA half-complete: fill write slot, then publish
auto* slot = dma.write_span().data();
adc_read_into(slot);
dma.publish();

// Task context: read stable consumer slot
process(dma.read_span().data());
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

**Parameter reference:** [docs/C_API_REFERENCE.md](docs/C_API_REFERENCE.md) — config fields, flags, callbacks, lifecycle, and all operations. Authoritative signatures: headers in `include/*.h`.

### Tiers

Controlled by `MEMKIT_C_API_FULL` and `MEMKIT_C_API_EXTENDED` in `memkit_config.h`:

**Tier 1 — always available (MCU + MPU)**

| Container | Header | Opaque size macro |
|-----------|--------|-------------------|
| Arena | `arena.h` | (struct fields visible) |
| Ring | `ring.h` | `MEMKIT_RING_OBJ_BYTES` (160) |
| Vector | `vector.h` | `MEMKIT_VECTOR_OBJ_BYTES` (192) |
| Stack | `stack.h` | `MEMKIT_STACK_OBJ_BYTES` (192) |
| Queue | `queue.h` | `MEMKIT_QUEUE_OBJ_BYTES` (192) |
| Bitset | `bitset.h` | `MEMKIT_BITSET_OBJ_BYTES` (128) |
| ObjPool | `objpool.h` | `MEMKIT_OBJPOOL_OBJ_BYTES` (192) |
| HandlePool | `handle_pool.h` | `MEMKIT_HANDLE_POOL_OBJ_BYTES` (128) |

**Tier 2 — full on MPU; MCU stubs link but return `*_ERR_UNSUPPORTED`**

| Container | Header | Opaque size macro |
|-----------|--------|-------------------|
| Deque | `deque.h` | `MEMKIT_DEQUE_OBJ_BYTES` (256) |
| HashMap | `hashmap.h` | `MEMKIT_HASHMAP_OBJ_BYTES` (512) |
| BTree | `btree.h` | `MEMKIT_BTREE_OBJ_BYTES` (512) |
| PQueue | `pqueue.h` | `MEMKIT_PQUEUE_OBJ_BYTES` (256) |
| List | `list.h` | `MEMKIT_LIST_OBJ_BYTES` (192) |
| DList | `dlist.h` | `MEMKIT_DLIST_OBJ_BYTES` (256) |
| LruCache | `lrucache.h` | `MEMKIT_LRUCACHE_OBJ_BYTES` (512) |

On MCU firmware that needs tier-2 containers, use the C++ API (`memkit.hpp`) with static or arena storage instead of the C stubs.

### Complete C API reference

Every C container follows the same conventions: `<name>_status_t`, `<name>_config_t`, opaque `<name>_t` blob, lifecycle (`*_init` / `*_create` / `*_deinit` / `*_destroy`), introspection (`*_size`, `*_capacity`, `*_empty`, `*_clear`), and `<name>_status_ok()`. Full per-function reference: [docs/C_API_REFERENCE.md](docs/C_API_REFERENCE.md).

**Arena** (`arena.h`) — tier 1, all targets

| Function | Purpose |
|----------|---------|
| `arena_init`, `arena_deinit` | Embed arena over caller buffer |
| `arena_create`, `arena_create_with_backing`, `arena_destroy` | MPU: heap or mmap backing |
| `arena_reset` | Free all bump allocations |
| `arena_alloc`, `arena_calloc` | Aligned bump allocation (absolute pointer alignment) |
| `arena_stats` | Capacity / used / remaining |

**Tier 1 containers** — MCU + MPU

| Container | Header | Key functions |
|-----------|--------|---------------|
| Ring | `ring.h` | `push_back`/`front`, `pop_*`, `peek_*`, `set_at`, `foreach`, `readable_contiguous`, `writable_contiguous`, `commit_read`/`write` |
| Queue | `queue.h` | `push`, `pop`, `peek_*`, `foreach`, contiguous + commit (same as ring) |
| Vector | `vector.h` | `reserve`, `push_back`, `pop_back`, `peek_*`, `set_at`, `at`, `foreach` |
| Stack | `stack.h` | `push`, `pop`, `peek`, `top`/`top_const`, `reserve`, `foreach` |
| Bitset | `bitset.h` | `test`/`set`/`reset`/`toggle`, `set_all`, `find_first_*`, set algebra, `copy`/`equal`, `load_bytes`/`store_bytes`, `foreach` |
| ObjPool | `objpool.h` | `alloc`, `alloc_copy`, `free`, `contains`, `index`, `foreach`, sizing helpers |
| HandlePool | `handle_pool.h` | `acquire`, `release`, `valid`, `get`, `handle_t`, sizing helpers |

**Tier 2 containers** — MPU full; MCU stubs return `*_ERR_UNSUPPORTED`

| Container | Header | Key functions |
|-----------|--------|---------------|
| Deque | `deque.h` | `push_*`, `pop_*`, `peek_*`, `front`/`back` pointers, `reserve`, `foreach` (no DMA contiguous API) |
| HashMap | `hashmap.h` | `put`, `get`, `remove`, `contains`, `foreach`; `hash_fn`, `key_eq_fn`; chaining or open addressing |
| BTree | `btree.h` | `insert`, `get`, `remove`, `contains`, `peek_min`/`max`, `foreach`; `compare_fn` |
| PQueue | `pqueue.h` | `push`, `pop`, `peek`, `top`/`top_const`, `reserve`, `foreach`; `compare_fn` |
| List | `list.h` | `push_*`, `pop_*`, `peek_*`, `insert_at`, `remove_at`, `remove_first`, `front`, `foreach` |
| DList | `dlist.h` | Same as list plus `back`, `foreach_reverse` |
| LruCache | `lrucache.h` | `get`, `peek`, `put`, `remove`, `contains`, `touch`, `foreach_mru`/`lru`; sizing helpers |

**Umbrella header:** `#include <memkit.h>` pulls `memkit_config.h`, all container headers above, and [`memkit_helpers.h`](include/memkit_helpers.h) (macros only — no extra link cost).

**Helpers** reduce init boilerplate on MCU:

```c
MEMKIT_ELEM_STORAGE(my_t, 16, buf);
MEMKIT_QUEUE_INIT_STATIC(&queue, my_t, buf);
MEMKIT_RETURN_VAL_IF_NOT_OK(queue_push(&queue, &item), -1);
```

See [C API reference](docs/C_API_REFERENCE.md) for the full function list, opaque sizes, and [`memkit_helpers.h`](include/memkit_helpers.h) macros.

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

**Ring and queue only** — `deque_t` has no contiguous read/write API. Both expose zero-copy DMA/UART windows:

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
| Stack | `Stack<T>` | `cstack_t` | 1 | Same core as vector |
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
| IntrusiveList | `IntrusiveListHead` | — | C++ only | Intrusive list heads |
| SpscQueue | `SpscQueue<T>` | — | C++ only | **Wait-free** SPSC — not `Queue` / `queue_t`; not Cortex-M0 |
| FlatMap | `FlatMap<K,V>` | — | C++ only | Sorted flat array map |
| TimerWheel | `TimerWheel<N>` | — | C++ only | Hashed timing wheel |
| DoubleBuffer | `DoubleBuffer<T>` | — | C++ only | **Wait-free** ping-pong block |
| MpscQueue | `MpscQueue<T>` | — | C++ only | **Lock-free** MPSC — one consumer; not Cortex-M0 |
| EnumMap | `EnumMap<Enum,V,N>` | — | C++ only | Enum-keyed array map |
| RingLog | `RingLog<Record>` | — | C++ only | Overwriting circular log |
| SparseSet | `SparseSet` | — | C++ only | Sparse set for active IDs |
| SmallBuffer | `SmallBuffer<N>` | — | C++ only | Binary payload buffer |
| FixedVariant | `FixedVariant<Ts...>` | — | C++ only | Tagged union |
| TokenBucket | `TokenBucket` | — | C++ only | Rate limiter |
| FixedIoVec | `FixedIoVec<N>` | — | C++ only | Scatter/gather slices |
| LookupTable | `LookupTable<X,Y>` | — | C++ only | Calibration / lookup table |
| BitReader | `BitReader` | — | C++ only | MSB-first bit reader |
| BitWriter | `BitWriter` | — | C++ only | MSB-first bit writer |
| MovingAverage | `MovingAverage<T,N>` | — | C++ only | Moving average window |
| WindowStats | `WindowStats<T,N>` | — | C++ only | Min/max/avg window |
| Arena | `memory::static_arena`, … | `arena_t` | 1 | Bump allocator |

---

## Project layout

```
include/
  memkit.h              C umbrella
  memkit_helpers.h      C init macros, storage sizing, status strings
  memkit_config.h       Target and feature flags
  memkit_object_sizes.h Opaque C object sizes
  *.h                   Per-container C headers
  memkit/
    memkit.hpp          C++ umbrella (32 utilities)
    stl.hpp             Zero-cost STL (MCU); optional heap aliases (MPU)
    containers/         C++ template wrappers
    detail/             Shared cores (internal)
    c_api/              C++ boxes, bridges, lifecycle helpers (internal)
    memory/             Arena, fixed buffer, heap, mmap (MPU)
src/
  arena.cpp
  mmap_backing.cpp
  c_api/
    bindings.cpp        Single TU for all C API extern "C" bindings
    bindings/*.inc.cpp  Per-container binding fragments (included, not compiled separately)
tests/
  test_*_cpp.cpp        C++ container tests (31 MCU + 1 MPU heap arena)
  test_*_c.c            C API per-container tests (8 MCU + 7 MPU tier 2)
  test_c_api_extended.c C API integration: arena_create + all *_create (MPU)
examples/
  example_mcu.cpp               C++ ring + arena (basic)
  example_mcu_c.c               C API ring + queue (tier 1, uses helpers)
  example_embedded_patterns.cpp DMA, MPSC, calibration, bit stream, filtering
  example_comm_pipeline.cpp     ByteRing RX + SPSC + TokenBucket pacing
  example_mpu.cpp               C++ MPU demo
  example_mpu.c                 C MPU demo (built as example_mpu_c)
docs/
  GETTING_STARTED.md            Tutorial: C/C++ paths, build, integration
  ADOPTION_GUIDE.md             STL vs memkit, MCU/MPU flags, ownership, piecemeal vendoring
  C_API_REFERENCE.md            C API: config fields, flags, function parameters
  CXX_API_REFERENCE.md            C++ API: init overloads, policies, methods
  DESIGN_PHILOSOPHY.md          Why MCU/MPU, memory models, C vs C++, STL policy
  CONTAINER_GUIDE.md            Which container? decision guide + recipes
  DISTRIBUTING_MCU_C.md         Prebuilt libmemkit_mcu_c.a for C firmware (no libstdc++)
benchmarks/
  bench_timing.cpp              Push/pop timing vs hand-rolled C
  hand_rolled/                  Minimal C ring + FIFO for comparison
  size/                         -Os size comparison binaries
```

## Choosing an API

**Prefer C++** when you can use C++26: typed elements, no manual `elem_size` / copy callbacks, all containers on MCU, and clearer ownership via move-only handles.

**Use C** when the translation unit must stay C, or you link memkit into an existing C firmware codebase. On MCU, limit yourself to tier 1 unless you also compile C++ and call `memkit.hpp` for heavier containers.

Both APIs call the same `detail/*_core` implementations; behavior matches when configuration is equivalent.

**Parity summary**

| C++ surface | C API | Notes |
|-------------|-------|-------|
| 14 sequential/map containers + `arena_t` | Full (tier 1 + tier 2 on MPU) | Same cores; C uses `void*` + callbacks |
| 18 embedded helpers (lock-free trio, `ByteRing`, …) | — | C++ only — see [cheat sheet](#container-cheat-sheet) |
| Tier 2 on MCU | Stubs link; `*_ERR_UNSUPPORTED` | Use C++ headers on MCU for hashmap, list, … |

### STL policy (`memkit/stl.hpp`)

| Build | Available via memkit | Blocked |
|-------|---------------------|---------|
| MCU | `array`, `span`, `optional`, `string_view`, `less`, `hash`, utilities | `memkit::stl::vector`, `memkit::stl::string`; `MEMKIT_USE_STL=1` errors |
| MPU (default) | Same zero-cost types as MCU | Heap aliases unless `MEMKIT_USE_STL=1` |
| MPU + `MEMKIT_USE_STL=1` | Also `vector`, `string` aliases | — |

You may still `#include <vector>` directly in your own MPU code; memkit just does not expose or depend on heap STL on MCU.

## CI

GitHub Actions (`.github/workflows/ci.yml`):

| Job | What it runs |
|-----|----------------|
| `mcu-ubuntu` | Clang 21 — `make all` + `lib-mcu-c` / `check-lib-mcu-c` / `test-lib-mcu-c-link` |
| `mcu-gcc-ubuntu` | GCC 14 (C++23) — `make all` + freestanding C lib targets |
| `mpu-ubuntu` | Clang 21 — `make mpu` (tier-2 C API + `test_heap_arena_cpp`) |
| `mpu-asan-ubuntu` | MPU + ASan/UBSan |
| `cmake-mcu-ubuntu` | CMake MCU — `ctest` |
| `cmake-ubuntu` | CMake MPU — `ctest` |
| `mpu-windows` | LLVM Clang on Windows — `-DMEMKIT_MPU=ON`, `ctest` (VirtualAlloc arena backing) |
| `benchmark-ubuntu` | `make benchmark` (timing + size) |
| `macos` | Apple Clang — MCU + MPU + benchmarks |

---

## Benchmarks

Compare memkit against minimal hand-rolled C implementations (`benchmarks/hand_rolled/`):

```bash
make benchmark              # default 200k push/pop iters per container
make benchmark BENCH_ITERS=500000
make benchmark-size         # -Os executable size only
```

Sample host output (Apple Silicon, Clang):

```
ring push/pop x50000: memkit … | hand-rolled C … | ratio ~3x
queue push/pop x50000: memkit … | hand-rolled C … | ratio ~3x
```

memkit is intentionally slightly slower than a one-off C ring — you trade a few ns/op for type safety, policies, contiguous DMA views, and shared cores. Size benchmarks link the full static library (arena + C API); for firmware, use only the containers you `#include` and LTO/`--gc-sections` to dead-strip.

---

## Examples (MCU)

| Example | Language | Demonstrates |
|---------|----------|--------------|
| `example_mcu.cpp` | C++ | Static ring + arena-backed ring |
| `example_mcu_c.c` | C | Tier-1 ring + queue with caller storage |
| `example_embedded_patterns.cpp` | C++ | DoubleBuffer, MpscQueue, LookupTable, bit stream, MovingAverage |
| `example_comm_pipeline.cpp` | C++ | ByteRing RX, SpscQueue, TokenBucket pacing |

MPU examples: `example_mpu.cpp` (C++ virtual-memory arena), `example_mpu.c` (C arena create + tier-2 create helpers). Built with `make mpu`. On Windows MPU, arena backing uses `VirtualAlloc`.

---

## Future work

Not required for the current v0.2 feature set; possible follow-ups if demand appears:

| Area | Description |
|------|-------------|
| **Exhaustive unit / fuzz / concurrency tests** | C++ and C API tests are happy-path smoke/integration coverage. Deeper work would add multi-threaded MPSC/SPSC stress tests, systematic error-path cases, fuzz/property tests, and shared C/C++ scenario equivalence checks. |
