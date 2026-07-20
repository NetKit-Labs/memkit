# memkit design philosophy

This document explains **why** memkit is shaped the way it is: two build targets, two API surfaces, several memory models, and a deliberate relationship to the C++ standard library. For API details see [README](../README.md); for first steps see [GETTING_STARTED.md](GETTING_STARTED.md).

---

## What memkit optimizes for

memkit is built for **embedded systems** where you care about:

- **Bounded memory** — sizes known at compile time or capped by explicit capacity
- **Deterministic behavior** — no hidden allocations on the hot path when you choose static or arena storage
- **One implementation per container family** — C and C++ share the same cores; behavior matches when configuration is equivalent
- **Optional dynamism on capable hosts** — heap and mmap exist only where the platform allows them (MPU)

It is **not** a full general-purpose standard library replacement. It is a **focused container kit** with a thin C layer for firmware that stays in C.

---

## One core, two APIs

```
C++  Ring<T>, Queue<T>, …     ──►  detail/*_core  ◄──  c_api/*_box  ──►  ring_t, queue_t, …
              typed policy                    shared algorithm              type-erased C
```

- **C++ API** — templates, typed elements, `memkit::status`, move-only handles
- **C API** — `elem_size`, caller `storage`, config structs, `*_status_t`

Both call the same `detail/*_core` logic. The C API does **not** wrap C++ container objects; it wraps the **same cores** with runtime type erasure (`void *`, copy/destroy callbacks when needed).

**Implication:** If a C++ test and a C test run the same operation sequence with equivalent config, results should match. Divergence usually means a binding bug (flags, callbacks), not two different algorithms.

---

## MCU vs MPU: two deployment models

memkit splits embedded work into **MCU** (bare-metal / RTOS firmware) and **MPU** (embedded Linux, edge gateway, capable host). **Both are first-class** — same 32 C++ utilities and shared cores; compile-time flags gate heap, virtual arena backing, and tier-2 C bindings. MCU is the **default Makefile target** because it is the most constrained environment, not because MPU is secondary.

| | **MCU** | **MPU** |
|--|---------|---------|
| **Typical platform** | Cortex-M, bare-metal, RTOS | Embedded Linux (Yocto, Buildroot), edge gateway, dev host |
| **Define** | `MEMKIT_MCU=1` | `MEMKIT_MPU=1` (+ `EMBEDDED_LINUX=1` on Linux; optional on Windows/macOS) |
| **Heap inside memkit** | off | on (`MEMKIT_ALLOW_HEAP=1`) |
| **Virtual arena backing** | off | on (`MEMKIT_ALLOW_MMAP=1`) — `mmap` or `VirtualAlloc` |
| **Default arena backing** | caller fixed buffer | virtual memory when enabled |
| **C API tier 2** | stubbed (`*_ERR_UNSUPPORTED`) | full |
| **C++ containers** | all 32 utilities | all 32 utilities |
| **Heap STL via memkit** | blocked (`MEMKIT_USE_STL=1` is a compile error) | optional off by default |

### Why two targets?

**MCU firmware** often has no `malloc`, no virtual memory, and strict flash/RAM budgets. Shipping hashmap/list/deque C bindings plus heap paths would bloat images and invite non-deterministic failure modes. Tier-1 C API covers the containers most C firmware needs; heavier containers remain available through **C++ headers** with static or arena storage.

**MPU services** on embedded Linux and MPU-class SoCs can use heap and virtual memory safely. Tier-2 C containers, `*_create` helpers, growable policies, and virtual-backed arenas (`mmap` / `VirtualAlloc`) become practical — typical for protocol stacks, logging daemons, and gateway services running Linux userspace.

### How to choose

```
Bare-metal / no heap in your platform port     →  MCU
Embedded Linux / MPU SoC with malloc + VM      →  MPU
Edge gateway or Linux userspace service        →  MPU
Pure C firmware, smallest tier-1 image         →  MCU + C API
Need every container in C on Linux             →  MPU + C API
Need every container on MCU                    →  MCU + C++ API (memkit.hpp)
Host dev / CI (Linux, macOS, Windows)          →  MPU (`-DMEMKIT_MPU=ON`)
```

Authoritative flags live in [`include/memkit_config.h`](../include/memkit_config.h) (included directly by all C container headers).

---

## Memory models: where bytes live

Containers never own heap storage **unless you ask** (MPU) or use growable/create paths. On MCU, the normal model is: **you provide bytes; memkit uses them.**

| Model | What you supply | When to use | MCU | MPU |
|-------|-----------------|-------------|-----|-----|
| **Fixed buffer** | Static array or stack buffer | Known max size; simplest | ✓ | ✓ |
| **Fixed pool** | Slab + free-list metadata | Same-sized objects, reuse slots (`ObjPool`, list node pools) | ✓ | ✓ |
| **Arena** | Bump allocator over your buffer | Several containers; reset together | ✓ | ✓ |
| **Heap** | `malloc` / growable reallocation | Unknown size at compile time | — | ✓ |
| **mmap** | OS-backed arena | Large MPU services | — | ✓ (POSIX `mmap` or Windows `VirtualAlloc`) |

### Fixed buffer (the 80% path)

```c
MEMKIT_ELEM_STORAGE(sensor_sample_t, 16, buf);
MEMKIT_QUEUE_INIT_STATIC(&q, sensor_sample_t, buf);
```

```cpp
memkit::stl::array<sensor_sample_t, 16> storage{};
memkit::Queue<sensor_sample_t> q;
q.init(storage);
```

Same idea: contiguous storage, fixed capacity, no allocator inside memkit.

### Arena (shared bump allocator)

An arena is a **sequential bump allocator** over a buffer you own (MCU) or over mmap/heap (MPU). Good when:

- Several containers are created in a batch
- You can **reset the whole arena** at once (end of frame, teardown phase)

Arena is **not** a replacement for fixed buffers on simple single-container cases — it adds a layer when sharing one backing region.

On MCU, default arena backing is **your fixed buffer** (`MEMKIT_DEFAULT_ARENA_BACKING = FIXED_BUFFER`), not mmap.

On MPU, arena backing can be a **caller fixed buffer**, **heap** (`heap_storage` + `heap_arena`), or **mmap** (`mmap_storage` + `mmap_arena`). See `tests/test_heap_arena_cpp.cpp` for heap-backed arena usage.

### Pool semantics

**Pool** in memkit means a **fixed slab with alloc/free of same-sized slots** — still static backing, not a separate runtime like `malloc`.

- **`ObjPool` / `HandlePool`** — the pool *is* the container (use these for fixed same-sized object pools; there is no separate `FixedPool` helper type)
- **`List` / `DList` / `BTree`** — node storage from a pre-sized pool (`*_policy::fixed_pool`)
- **HashMap chaining (MPU)** — nodes may come from arena/heap bump; a dedicated fixed node pool is a possible future tightening for hard caps

“Fixed pool” in the README memory table is **pool-shaped static storage**, not a fifth global allocator beside arena.

### Growable (MPU)

Vector, queue, deque, pqueue, hashmap can **double capacity** when full. On MPU, growth may use heap if no arena was supplied. On MCU, growable + heap paths are disabled — use fixed capacity or arena-backed init with a known upper bound.

### Bounds and sizing (what memkit checks vs what you own)

memkit is built for firmware teams who **choose capacity up front** and **check status codes** — not for treating every init as untrusted input.

**The library guarantees:**

- Containers do not silently grow past **fixed capacity** on MCU; push/pop when full or empty returns an explicit status.
- **Arena** bump allocation validates offset, alignment padding, and remaining space (see `arena_alloc` / C++ `memory::arena::allocate`). Alignment is applied to the **absolute address** of the next slot (base + offset), so a backing buffer that is not itself aligned can still satisfy `alignof(std::atomic<…>)` and similar requirements — at the cost of a few bytes of internal padding.
- Obvious bad config is rejected (`capacity == 0`, null storage, invalid alignment).

**You own:**

- Picking **element count and buffer sizes** that fit your product (prefer `MEMKIT_ELEM_STORAGE`, `stl::array<T, N>`, or documented `storage_bytes`).
- Ensuring **`elem_size × capacity`** (and your arena backing size) is a sane, representable size for your platform — memkit does not exhaustively guard every pathological `size_t` multiply on every init path; static sizing makes this a compile-time non-issue.
- Handling **`full`**, **`oom`**, and **`invalid`** at call sites.

This is intentional: less defensive wrapping around absurd sizes, more explicit contracts for embedded use.

---

## Concurrency and RTOS (zero-overhead, OS-agnostic)

memkit is architected **without internal mutexes or RTOS coupling** — a deliberate embedded advantage: maximum hot-path speed, zero flash spent on unused lock machinery, and portability across bare-metal, RTOS, and embedded Linux.

Containers split into **two categories**:

| Category | Types | Model |
|----------|-------|-------|
| **Lock-free utilities** (C++ only) | `SpscQueue`, `MpscQueue`, `DoubleBuffer` | Wait-free or lock-free handoff with fixed producer/consumer roles |
| **Single-threaded containers** | Everything else (C and C++) | One owning context; integrator wraps with OS mutex/semaphore if shared |

**Lock-free trio** ([`spsc_queue.hpp`](../include/memkit/containers/spsc_queue.hpp), [`mpsc_queue.hpp`](../include/memkit/containers/mpsc_queue.hpp), [`double_buffer.hpp`](../include/memkit/containers/double_buffer.hpp)):

| Type | Guarantee | Use |
|------|-----------|-----|
| `SpscQueue` | Wait-free | One producer → one consumer (ISR ↔ task) |
| `MpscQueue` | Lock-free | Many producers → **one** consumer |
| `DoubleBuffer` | Wait-free | Ping-pong block via `publish()` |

**Not supported on ARM Cortex-M0/M0+** for the lock-free trio (no hardware exclusive instructions); all **single-threaded** containers work on M0+. See hardware table in [CONCURRENCY.md](CONCURRENCY.md#hardware--isa-compatibility-lock-free-trio-only).

C `queue_t` / `ring_t` and C++ `Queue` / `Ring` are **single-threaded** — not ISR-safe despite naming.

**You own:** OS locks when sharing single-threaded containers across tasks; role discipline for the lock-free trio. Full patterns (FreeRTOS, wrapper templates, ISA matrix): **[CONCURRENCY.md](CONCURRENCY.md)**.

---

## C API tiers

| Tier | Containers | MCU C | MPU C |
|------|------------|-------|-------|
| **1** | arena, ring, queue, vector, stack, bitset, objpool, handle_pool | full | full |
| **2** | hashmap, btree, pqueue, list, dlist, lrucache, deque | stubs link; return `*_ERR_UNSUPPORTED` | full |

**Rationale:** Keep the default C firmware link small. Headers for tier 2 still exist on MCU so mixed links succeed; calls fail at runtime with a clear status unless you move to MPU or use **C++** on MCU for those containers.

---

## C++ and the standard library

memkit is **not** “STL-free.” It is **heap-STL-averse on MCU**.

### What memkit uses from the standard library

On **both** MCU and MPU, via [`memkit/stl.hpp`](../include/memkit/stl.hpp):

| Facility | Role | Heap at link time? |
|----------|------|--------------------|
| `std::array`, `std::span` | Static storage and views | No |
| `std::optional` | `try_pop`-style returns | No |
| `std::string_view` | Non-owning text | No |
| `std::less`, `std::hash`, … | Comparisons / hashing | No |
| `<type_traits>`, `<utility>` | Move, forward, concepts | No |
| `<new>` | **Placement** new into your buffers | No |
| `std::atomic` | SPSC/MPSC/DoubleBuffer | Sometimes `-latomic` on embedded GCC |

**Blocked on MCU:** `memkit::stl::vector` and `memkit::stl::string` (and `MEMKIT_USE_STL=1`).

### Compile time vs link time

- **Compile time (C++ + `memkit.hpp`):** You need a C++ toolchain with standard **headers** (`<array>`, `<span>`, …). Compile with **`-fno-exceptions -fno-rtti`** (enforced by the Makefile and CMake).
- **Link time:** `std::array` and friends are header-only — they do **not** pull in heap STL. Linking libstdc++/libc++ is mainly driven by **compiled C++ in memkit’s `.a`**, not by exceptions (memkit does not use them).

### Pure C firmware path

For customers who **only** use the C API:

1. Include `<memkit.h>` — no C++ headers in **your** `.c` files
2. Link [`libmemkit_mcu_c.a`](../docs/DISTRIBUTING_MCU_C.md) built with `-fno-exceptions -fno-rtti -ffreestanding`
3. Link with **`cc`**, not `c++`; **no `-lstdc++`** required for tier-1 C API

The prebuilt archive contains C++ **implementation** (bindings), but your firmware stays C.

### C++ storage vs C storage

The C API sees `void *` + `storage_bytes`. In a **C++** translation unit you may back it with `std::array` or a C array — layout is the same. In **`.c`** files, use C arrays or `MEMKIT_ELEM_STORAGE` macros.

---

## Error handling philosophy

- Operations return **`memkit::status`** / `*_status_t` — no exceptions on container paths (and the project builds with **`-fno-exceptions`**)
- **`memkit::ok(st)`** / `*_status_ok(st)` at every init and mutating call
- Failures are **explicit**: `empty`, `full`, `oom`, `unsupported`, `not_found`
- Debug helpers: `memkit_*_status_string` in [`memkit_helpers.h`](../include/memkit_helpers.h)

Embedded code should assume failures happen (full queue, out of arena space) and handle them.

---

## Ownership and lifecycle

| Pattern | C | C++ |
|---------|---|-----|
| Embed in struct / stack | `*_init` + `*_deinit` | `init` + destructor |
| Library allocates backing | `*_create` + `*_destroy` (MPU) | `init_from_arena`, move-only |

C opaque blobs (`ring_t.bytes[MEMKIT_RING_OBJ_BYTES]`) hide layout; sizes are checked at library build time via static asserts.

**Rule of thumb:** `init`/`deinit` pairs with caller storage; `create`/`destroy` when memkit or arena owns allocation (MPU).

---

## What memkit deliberately is not

- **Not** a host desktop framework — MPU support targets embedded Linux and similar MPU hosts (including Windows with VirtualAlloc arena backing); MCU remains bare-metal/RTOS focused
- **Not** exception-driven — status codes throughout
- **Not** implicitly growable on MCU — capacity is a contract
- **Not** thread-safe by default — lock-free trio (wait-free / lock-free) plus single-threaded containers; see [CONCURRENCY.md](CONCURRENCY.md)
- **Not** RTOS-coupled — no built-in mutex layer; integrator-owned OS synchronization for shared single-threaded types
- **Not** one-size API surface in C — tier 1 stays small; full C++ surface lives in headers

---

## Design checklist for integrators

1. Pick **MCU or MPU** from your platform, not from container preference alone.
2. Prefer **fixed buffer** until you need arena sharing or MPU growable.
3. **Size storage explicitly** — memkit enforces capacity at runtime but expects sane init sizes from you (see [bounds and sizing](#bounds-and-sizing-what-memkit-checks-vs-what-you-own)).
4. **C firmware** → tier-1 C API + optional `libmemkit_mcu_c.a`.
5. **C++ firmware on MCU** → `memkit.hpp` + static/`std::array`; all 32 utilities available.
6. **Embedded Linux / gateway in C or C++** → MPU build; tier-2 C API + virtual/heaps arenas.
7. **Heavy maps/lists in C on MCU** → not in tier-1 C; use C++ API or MPU build.
8. **RTOS / ISR sharing** → read [CONCURRENCY.md](CONCURRENCY.md); default is single-context; use `SpscQueue` / `MpscQueue` / `DoubleBuffer` for handoff, or your mutex.
9. Always check **status**; use helpers and tests as templates.

---

## Related docs

- [CONCURRENCY.md](CONCURRENCY.md) — thread/ISR contract, lock-free trio, FreeRTOS patterns
- [ADOPTION_GUIDE.md](ADOPTION_GUIDE.md) — STL vs memkit containers, build flags, ownership, piecemeal integration
- [GETTING_STARTED.md](GETTING_STARTED.md) — hands-on paths
- [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) — which container for which job
- [DISTRIBUTING_MCU_C.md](DISTRIBUTING_MCU_C.md) — prebuilt C library without libstdc++ link
- [README § Targets](../README.md#targets) — quick reference tables
- [README § Examples](../README.md#examples) — MCU and MPU runnable demos
