# Software Verification and Performance Report — MemKit

**Project:** MemKit — embedded C/C++ container and memory-utility library  
**Evidence reviewed:** 2026-10-25  
**Baseline:** `main` commit `9168fbb27f696d4c84257e26ae72ff4e2c0ef1e6` (before this documentation addition)  
**Document type:** Retrospective engineering evidence review; **not** a new benchmark or target-hardware test

## 1. Purpose, scope and disposition

MemKit provides **32 C++ utilities** and **14 C containers** using shared underlying container cores where a C binding exists. It supports two compile-time deployment profiles: **MCU**, favoring caller-owned static buffers, fixed pools and arenas without library heap allocation; and **MPU**, adding optional heap, growable storage and virtual-memory-backed arenas.

**Disposition:** The reviewed baseline passed its **10-job GitHub Actions run** and includes functional tests, C/C++ builds, sanitizer and static-analysis gates, freestanding C-link checks, and measured **host** timing/size comparisons. These establish a useful software-verification baseline, **not** a validated flash/RAM or execution-time guarantee on a microcontroller. See the [baseline CI run](https://github.com/NetKit-Labs/memkit/actions/runs/40200135115), [README](../README.md) and [design rationale](DESIGN_PHILOSOPHY.md).

## 2. Verification configuration and requirements traceability

| ID | Intended behavior / requirement | Existing test or check | Evidence classification |
| --- | --- | --- | --- |
| SW-01 | Container behavior (FIFO, ring wrap, maps, pools, trees, utilities) | **32 C++ test executables** in the default MCU build; [test sources](../tests/) and [Makefile](../Makefile) | Automated host functional tests |
| SW-02 | Stable C API and equivalent core behavior | **8 MCU C API test executables**; MPU also exercises tier-2 C tests and extended API test; C and C++ bindings share `detail/*_core` implementations | Automated API checks; no formal equivalence proof |
| SW-03 | Caller-owned storage, arenas, fixed pools | C and C++ arena, ring, object/handle pool and other container tests; examples with static storage | Host functional tests of storage contracts |
| SW-04 | MPU optional heap and virtual backing | `test_heap_arena_cpp`, extra MPU C tests, Linux/macOS MPU paths, Windows MPU CMake tests | Host/OS verification; **not** physical embedded-Linux-board validation |
| SW-05 | MCU C distributable without a C++ runtime link | `make lib-mcu-c check-lib-mcu-c test-lib-mcu-c-link`, including undefined-symbol checks and linking with the C driver | Host-toolchain link verification; not an Arm-target firmware image |
| SW-06 | C++ builds with exceptions and RTTI disabled | Make/CMake compile flags `-fno-exceptions -fno-rtti` (MSVC option handling in CMake) | Build-policy verification |
| SW-07 | Detect errors in memory and undefined behavior on exercised MPU paths | `mpu-asan-ubuntu` uses AddressSanitizer and UndefinedBehaviorSanitizer | Dynamic host test gate; limited to exercised paths |
| SW-08 | Static source inspection | `make cppcheck` across `src/`, `tests/`, `examples/` and `benchmarks/` | Cppcheck gate; not a proof of defect absence |
| SW-09 | Compiler and operating-system portability | Clang 21 C++26; GCC 14 C++23; C23; Linux, macOS and Windows paths; Make and CMake/CTest | CI build/test matrix |
| SW-10 | Quantitative ring/queue overhead and linked sizes | Host benchmark jobs run `make benchmark BENCH_ITERS=50000` | **Measured host** comparison, detailed below |

The MCU vs MPU feature boundary is intentional. For example, **tier-2 C APIs are not fully available on MCU**, whereas C++ utilities remain available subject to their contracts. The `SpscQueue`, `MpscQueue`, and `DoubleBuffer` concurrency utilities have specified role/atomic requirements; ordinary `Queue`, `Ring`, maps and the C API are **not automatically thread- or ISR-safe**. See [container guide](CONTAINER_GUIDE.md) and [concurrency contract](CONCURRENCY.md).

The **40 default MCU test executables** (32 C++ and 8 C) are *executables/suites*, not 40 individual assertions or a line/branch-coverage percentage. The latest pre-report [CI baseline](https://github.com/NetKit-Labs/memkit/actions/runs/40200135115) completed with all 10 jobs successful.

## 3. Recorded timing performance — host results

The benchmark measures repeated **push immediately followed by pop** of a single integer, comparing MemKit `Ring<int>` / `Queue<int>` to small hand-written C ring/FIFO implementations. Both use **256-element backing buffers**, with **50,000 push/pop pairs (100,000 operations)** in the retained CI runs. The source uses `std::chrono::steady_clock`, divides total elapsed time by two operations per pair and retains an observable sink. Timing numbers are small and subject to timer, compiler, runtime and scheduling variation. [Benchmark source](../benchmarks/bench_timing.cpp).

| Platform / CI job | Operation | MemKit | Hand-written C | MemKit ÷ C |
| --- | --- | ---: | ---: | ---: |
| Ubuntu, Clang 21 | Ring push/pop | **0.91 ms**, ~9 ns/op | **0.25 ms**, ~2 ns/op | **3.68×** |
| Ubuntu, Clang 21 | Queue push/pop | **0.83 ms**, ~8 ns/op | **0.25 ms**, ~2 ns/op | **3.35×** |
| macOS, Apple Clang | Ring push/pop | **1.41 ms**, ~14 ns/op | **0.46 ms**, ~5 ns/op | **3.03×** |
| macOS, Apple Clang | Queue push/pop | **1.31 ms**, ~13 ns/op | **0.46 ms**, ~5 ns/op | **2.87×** |

**Result:** The hand-written C examples are **faster** on this narrow host microbenchmark. The added MemKit abstraction is a deliberate tradeoff for typed APIs, storage policies and shared implementations. Neither the absolute host times nor these ratios establish ARM Cortex-M performance, worst-case execution time, interrupt safety or an application-level speedup.

**Traceability:** The numeric measurements were extracted from the successful [2026-10-25 baseline run](https://github.com/NetKit-Labs/memkit/actions/runs/40200135115), specifically its **benchmark-ubuntu** and **macos** job logs. The README also notes the comparison is host-only and includes illustrative, not normative, ratios.

## 4. Recorded executable-size comparison — host results

The benchmark compiles four small linked executables with `-Os`, per-function/per-data sections and dead-section elimination (`--gc-sections` on Linux, `-dead_strip` on macOS). The stored CI output prints the **linked executable file size in bytes**, *not* the target MCU's flash or RAM footprint.

| Platform | MemKit ring | C ring | MemKit queue | C queue |
| --- | ---: | ---: | ---: | ---: |
| Ubuntu, Clang 21 | **131,656 B** | **15,952 B** | **131,656 B** | **15,936 B** |
| macOS, Apple Clang | **34,256 B** | **33,704 B** | **34,256 B** | **33,696 B** |

The Linux results show substantially larger MemKit linked binaries in this particular host configuration; the macOS results are much closer. **These are not comparable across operating systems as firmware footprint data.** They reflect the selected host link environment, runtime and linked-library effects, the concrete example and linker flags. The size recipe links MemKit's static library objects; it is not a minimal cross-compiled ARM `.elf` / `.map` assessment. No code-size or stack-headroom superiority over handwritten C is claimed. [Makefile benchmark target](../Makefile), [README benchmarks](../README.md#benchmarks), [baseline CI logs](https://github.com/NetKit-Labs/memkit/actions/runs/40200135115).

## 5. CI coverage and repeatability

The successful baseline GitHub workflow contains the following **10 jobs**:

| Job | Gate |
| --- | --- |
| `cppcheck` | Static analysis |
| `mcu-ubuntu` | Clang 21 MCU-profile builds/tests + C-only archive/link |
| `mcu-gcc-ubuntu` | GCC 14 C++23 and C23 MCU-profile builds/tests + C-only archive/link |
| `mpu-ubuntu` | Clang 21 MPU-profile builds/tests |
| `mpu-asan-ubuntu` | MPU AddressSanitizer + UndefinedBehaviorSanitizer |
| `cmake-mcu-ubuntu` | CMake MCU build + CTest |
| `cmake-ubuntu` | CMake MPU build + CTest |
| `mpu-windows` | Windows Clang/CMake MPU build + CTest |
| `benchmark-ubuntu` | Host timing + size measurements |
| `macos` | MCU/MPU builds/tests + host benchmarks |

Primary local reproduction targets:

```sh
make all
make mpu
make lib-mcu-c check-lib-mcu-c test-lib-mcu-c-link
make cppcheck
make benchmark BENCH_ITERS=50000
make benchmark-size
# Alternate CMake/CTest paths are specified in the README and CMakeLists.txt.
```

Compilers, platform dependencies, build flags and logs matter. CI runs on hosted machines using **MCU build configurations**, not on a physical Cortex-M device. Both Make and CMake definitions should be reviewed before claiming support for a specific cross-compiler/ABI.

## 6. Limitations, unresolved evidence and recommended next measurements

- **No retained actual-target hardware timing, energy, stack watermarks, MCU code-size map, or Cortex-M flash/RAM measurements** were identified. This is a *Software* Verification and Performance Report, deliberately not a Hardware Verification Report.
- No formal proof of C/C++ behavioral equivalence, deterministic worst-case latency, memory safety for all inputs or ISR interleavings, or exhaustive concurrency verification is claimed.
- No measured line/branch coverage percentage, ThreadSanitizer run or multi-day stress/soak result is presented in the reviewed CI configuration.
- Native lock-free progress depends on the selected MCU's atomic support, ABI and producer/consumer contract; successful hosted tests do not prove an ISR-safe deployment on every target. In particular, [concurrency documentation](CONCURRENCY.md) excludes the lock-free trio for Cortex-M0/M0+.
- Existing timing tests primarily measure one ring/queue usage pattern with warm host processes and do not benchmark every container or capture latency distributions.

**Highest-value future measured evidence:** (1) cross-compile representative C and C++ MCU examples for STM32F446RE, archive the exact `arm-none-eabi-size`/`.map` output and per-API image delta; (2) run on-device latency/throughput tests with interrupts and optimizer flags identified; (3) benchmark arena/fixed-pool paths against a documented C baseline with repeated statistical samples; and (4) add targeted concurrent stress tests and recorded coverage reports where appropriate.

**Integrity statement:** All numbers above predate this document and come from the cited tracked source or CI logs. No hardware run, new benchmark, new coverage measurement or production certification was performed to create this report.
