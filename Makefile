CXX ?= c++
CC ?= cc
# No exceptions or RTTI anywhere in memkit (library, tests, examples, benchmarks).
MEMKIT_CXX_NO_EH_RTTI = -fno-exceptions -fno-rtti
CXXFLAGS ?= -std=c++26 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MCU=1 $(MEMKIT_CXX_NO_EH_RTTI)
CFLAGS ?= -std=c23 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MCU=1
LDFLAGS ?=

LIB_OBJS = $(BUILD)/arena.o $(BUILD)/mmap_backing.o $(BUILD)/c_api/bindings.o
BUILD = build

CPP_TESTS = test_ring_cpp test_vector_cpp test_hashmap_cpp test_list_cpp \
              test_dlist_cpp test_btree_cpp test_stack_queue_cpp test_pqueue_cpp \
              test_objpool_cpp test_bitset_cpp test_deque_cpp test_lrucache_cpp \
              test_small_string_cpp test_byte_ring_cpp test_handle_pool_cpp \
              test_intrusive_list_cpp test_spsc_queue_cpp test_flat_map_cpp test_timer_wheel_cpp \
              test_double_buffer_cpp test_mpsc_queue_cpp test_enum_map_cpp test_ring_log_cpp \
              test_sparse_set_cpp test_small_buffer_cpp test_fixed_variant_cpp test_token_bucket_cpp \
              test_fixed_iovec_cpp test_lookup_table_cpp test_bit_stream_cpp test_running_stats_cpp \
              test_arena_cpp

MCU_EXAMPLES = example_mcu example_embedded_patterns example_comm_pipeline
MCU_C_EXAMPLES = example_mcu_c

BENCH_ITERS ?= 200000
BENCH_DIR = benchmarks
HAND_ROLLED_OBJS = $(BUILD)/bench_hand_rolled_ring.o $(BUILD)/bench_hand_rolled_fifo.o
SIZE_FLAGS = -Os -ffunction-sections -fdata-sections
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SIZE_LDFLAGS = $(LDFLAGS) -Wl,-dead_strip
else
SIZE_LDFLAGS = $(LDFLAGS) -Wl,--gc-sections
endif

C_API_MCU_TESTS = test_arena_c test_ring_c test_queue_c test_vector_c test_stack_c \
                  test_bitset_c test_objpool_c test_handle_pool_c
C_API_MPU_TESTS = test_deque_c test_hashmap_c test_btree_c test_pqueue_c test_list_c \
                  test_dlist_c test_lrucache_c
MPU_CPP_TESTS = test_heap_arena_cpp

MPU_CXXFLAGS = -std=c++26 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 \
               -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1 $(MEMKIT_CXX_NO_EH_RTTI)
MPU_CFLAGS = -std=c23 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 \
             -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1
MPU_OBJS = $(BUILD)/mpu/arena.o $(BUILD)/mpu/mmap_backing.o $(BUILD)/mpu/c_api/bindings.o

CPPCHECK ?= cppcheck
CPPCHECK_DIRS = src tests examples benchmarks

.PHONY: all test_cpp test_c_api test_c_api_mpu test_c_api_extended test_cpp_mpu mcu mpu clean lib \
	lib-mcu-c check-lib check-lib-mcu-c test-lib-mcu-c-link benchmark benchmark-size cppcheck

# Freestanding MCU C API archive (no libstdc++ at link time for C customers).
MCU_C_CXXFLAGS = $(CXXFLAGS) -ffreestanding
MCU_C_LIB_DIR = $(BUILD)/mcu_c
MCU_C_LIB_OBJS = $(MCU_C_LIB_DIR)/arena.o $(MCU_C_LIB_DIR)/c_api/bindings.o
MCU_C_LIB = $(BUILD)/libmemkit_mcu_c.a

all: lib test_cpp test_c_api mcu

lib: $(BUILD)/c_api $(LIB_OBJS)

lib-mcu-c: $(MCU_C_LIB)

$(MCU_C_LIB): $(MCU_C_LIB_OBJS) | $(BUILD) $(BUILD)/mcu_c
	ar rcs $@ $^
	@echo "==> $@ (freestanding tier-1 C API; link with cc, not libstdc++)"

$(MCU_C_LIB_DIR)/%.o: src/%.cpp | $(MCU_C_LIB_DIR)
	$(CXX) $(MCU_C_CXXFLAGS) -c -o $@ $<

$(MCU_C_LIB_DIR)/c_api/%.o: src/c_api/%.cpp | $(BUILD)
	mkdir -p $(MCU_C_LIB_DIR)/c_api
	$(CXX) $(MCU_C_CXXFLAGS) -c -o $@ $<

check-lib: lib
	@bad=$$(nm -g $(LIB_OBJS) 2>/dev/null | grep -E '__cxa_|__dynamic_cast|typeinfo' || true); \
	if [ -n "$$bad" ]; then \
		echo "error: exception/RTTI symbols in $(LIB_OBJS):"; \
		echo "$$bad"; \
		exit 1; \
	fi
	@echo "check-lib: ok (no exception/RTTI symbols in library objects)"

check-lib-mcu-c: $(MCU_C_LIB)
	@bad=$$(nm -u $(MCU_C_LIB) 2>/dev/null | grep -E '__cxa_|__gxx_|_ZSt' || true); \
	if [ -n "$$bad" ]; then \
		echo "error: C++ runtime symbols in $(MCU_C_LIB):"; \
		echo "$$bad"; \
		exit 1; \
	fi
	@echo "check-lib-mcu-c: ok (no libstdc++ linkage symbols)"

test-lib-mcu-c-link: lib-mcu-c
	$(CC) $(CFLAGS) -c -o $(BUILD)/example_mcu_c_libtest.o examples/example_mcu_c.c
	$(CC) -o $(BUILD)/example_mcu_c_libtest $(BUILD)/example_mcu_c_libtest.o $(MCU_C_LIB) $(LDFLAGS)
	./$(BUILD)/example_mcu_c_libtest
	@echo "test-lib-mcu-c-link: ok (linked with cc only)"

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/c_api/%.o: src/c_api/%.cpp | $(BUILD)/c_api
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test_cpp: $(addprefix $(BUILD)/,$(CPP_TESTS))
	@for t in $(CPP_TESTS); do echo "==> $$t"; ./$(BUILD)/$$t || exit 1; done

test_c_api: $(addprefix $(BUILD)/,$(C_API_MCU_TESTS))
	@for t in $(C_API_MCU_TESTS); do echo "==> $$t"; ./$(BUILD)/$$t || exit 1; done

test_c_api_mpu: $(addprefix $(BUILD)/mpu/,$(C_API_MPU_TESTS))
	@for t in $(C_API_MPU_TESTS); do echo "==> $$t"; ./$(BUILD)/mpu/$$t || exit 1; done

test_cpp_mpu: $(addprefix $(BUILD)/mpu/,$(MPU_CPP_TESTS))
	@for t in $(MPU_CPP_TESTS); do echo "==> $$t"; ./$(BUILD)/mpu/$$t || exit 1; done

test_c_api_extended: $(BUILD)/test_c_api_extended
	./$(BUILD)/test_c_api_extended

define CPP_TEST_RULE
$(BUILD)/$(1): tests/$(1).cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) tests/$(1).cpp $(LIB_OBJS) -o $$@ $(LDFLAGS)
endef

$(foreach t,$(CPP_TESTS),$(eval $(call CPP_TEST_RULE,$(t))))

define C_API_MCU_TEST_RULE
$(BUILD)/$(1).o: tests/$(1).c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $$@ $$<
$(BUILD)/$(1): $(BUILD)/$(1).o $(LIB_OBJS) | $(BUILD)
	$(CXX) -o $$@ $(BUILD)/$(1).o $(LIB_OBJS) $(LDFLAGS)
endef

$(foreach t,$(C_API_MCU_TESTS),$(eval $(call C_API_MCU_TEST_RULE,$(t))))

define C_API_MPU_TEST_RULE
$(BUILD)/mpu/$(1).o: tests/$(1).c | $(BUILD)/mpu
	$(CC) $(MPU_CFLAGS) -c -o $$@ $$<
$(BUILD)/mpu/$(1): $(BUILD)/mpu/$(1).o $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $$@ $(BUILD)/mpu/$(1).o $(MPU_OBJS) $(LDFLAGS)
endef

$(foreach t,$(C_API_MPU_TESTS),$(eval $(call C_API_MPU_TEST_RULE,$(t))))

define MPU_CPP_TEST_RULE
$(BUILD)/mpu/$(1): tests/$(1).cpp $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) tests/$(1).cpp $(MPU_OBJS) -o $$@ $(LDFLAGS)
endef

$(foreach t,$(MPU_CPP_TESTS),$(eval $(call MPU_CPP_TEST_RULE,$(t))))

$(BUILD)/test_c_api_extended.o: tests/test_c_api_extended.c | $(BUILD)/mpu
	$(CC) $(MPU_CFLAGS) -c -o $@ $<

$(BUILD)/test_c_api_extended: $(BUILD)/test_c_api_extended.o $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $@ $(BUILD)/test_c_api_extended.o $(MPU_OBJS) $(LDFLAGS)

mcu: $(addprefix $(BUILD)/,$(MCU_EXAMPLES)) $(addprefix $(BUILD)/,$(MCU_C_EXAMPLES))
	@for e in $(MCU_EXAMPLES) $(MCU_C_EXAMPLES); do echo "==> $$e"; ./$(BUILD)/$$e || exit 1; done

mpu: $(BUILD)/mpu/c_api $(BUILD)/example_mpu $(BUILD)/example_mpu_c test_c_api_mpu test_c_api_extended test_cpp_mpu
	./$(BUILD)/example_mpu
	./$(BUILD)/example_mpu_c

$(BUILD)/example_mcu: examples/example_mcu.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ examples/example_mcu.cpp $(LIB_OBJS) $(LDFLAGS)

$(BUILD)/example_embedded_patterns: examples/example_embedded_patterns.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ examples/example_embedded_patterns.cpp $(LIB_OBJS) $(LDFLAGS)

$(BUILD)/example_comm_pipeline: examples/example_comm_pipeline.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ examples/example_comm_pipeline.cpp $(LIB_OBJS) $(LDFLAGS)

$(BUILD)/example_mcu_c.o: examples/example_mcu_c.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/example_mcu_c: $(BUILD)/example_mcu_c.o $(LIB_OBJS) | $(BUILD)
	$(CXX) -o $@ $(BUILD)/example_mcu_c.o $(LIB_OBJS) $(LDFLAGS)

$(BUILD)/bench_hand_rolled_ring.o: $(BENCH_DIR)/hand_rolled/ring_buffer.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(BENCH_DIR)/hand_rolled -c -o $@ $<

$(BUILD)/bench_hand_rolled_fifo.o: $(BENCH_DIR)/hand_rolled/fifo_queue.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(BENCH_DIR)/hand_rolled -c -o $@ $<

$(BUILD)/bench_timing: $(BENCH_DIR)/bench_timing.cpp $(HAND_ROLLED_OBJS) $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(BENCH_DIR) $(BENCH_DIR)/bench_timing.cpp $(HAND_ROLLED_OBJS) $(LIB_OBJS) -o $@ $(LDFLAGS)

$(BUILD)/size_ring_memkit: $(BENCH_DIR)/size/ring_memkit.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(SIZE_FLAGS) $(BENCH_DIR)/size/ring_memkit.cpp $(LIB_OBJS) -o $@ $(SIZE_LDFLAGS)

$(BUILD)/size_ring_c: $(BENCH_DIR)/size/ring_hand_rolled.c $(BUILD)/bench_hand_rolled_ring.o | $(BUILD)
	$(CC) $(CFLAGS) $(SIZE_FLAGS) -I$(BENCH_DIR) $(BENCH_DIR)/size/ring_hand_rolled.c $(BUILD)/bench_hand_rolled_ring.o -o $@ $(SIZE_LDFLAGS)

$(BUILD)/size_queue_memkit: $(BENCH_DIR)/size/queue_memkit.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(SIZE_FLAGS) $(BENCH_DIR)/size/queue_memkit.cpp $(LIB_OBJS) -o $@ $(SIZE_LDFLAGS)

$(BUILD)/size_queue_c: $(BENCH_DIR)/size/queue_hand_rolled.c $(BUILD)/bench_hand_rolled_fifo.o | $(BUILD)
	$(CC) $(CFLAGS) $(SIZE_FLAGS) -I$(BENCH_DIR) $(BENCH_DIR)/size/queue_hand_rolled.c $(BUILD)/bench_hand_rolled_fifo.o -o $@ $(SIZE_LDFLAGS)

benchmark: $(BUILD)/bench_timing benchmark-size
	@echo "==> timing (iters=$(BENCH_ITERS))"
	MEMKIT_BENCH_ITERS=$(BENCH_ITERS) ./$(BUILD)/bench_timing

benchmark-size: $(BUILD)/size_ring_memkit $(BUILD)/size_ring_c $(BUILD)/size_queue_memkit $(BUILD)/size_queue_c
	@echo "==> binary size (-Os, linked executable bytes)"
	@for f in $(BUILD)/size_ring_memkit $(BUILD)/size_ring_c $(BUILD)/size_queue_memkit $(BUILD)/size_queue_c; do \
		printf "  %s: " "$$f"; wc -c < "$$f" | tr -d ' '; \
	done
	@echo "note: memkit binaries include libmemkit (arena + C API); hand-rolled C is standalone"

$(BUILD)/mpu/%.o: src/%.cpp | $(BUILD)/mpu
	$(CXX) $(MPU_CXXFLAGS) -c -o $@ $<

$(BUILD)/mpu/c_api/%.o: src/c_api/%.cpp | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -c -o $@ $<

$(BUILD)/example_mpu: examples/example_mpu.cpp $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $@ examples/example_mpu.cpp $(MPU_OBJS) $(LDFLAGS)

$(BUILD)/example_mpu_c.o: examples/example_mpu.c | $(BUILD)/mpu
	$(CC) $(MPU_CFLAGS) -c -o $@ $<

$(BUILD)/example_mpu_c: $(BUILD)/example_mpu_c.o $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $@ $(BUILD)/example_mpu_c.o $(MPU_OBJS) $(LDFLAGS)

$(BUILD)/mpu/c_api:
	mkdir -p $(BUILD)/mpu/c_api

$(BUILD)/c_api:
	mkdir -p $(BUILD)/c_api

$(BUILD)/mcu_c/c_api:
	mkdir -p $(BUILD)/mcu_c/c_api

$(BUILD)/mcu_c:
	mkdir -p $(BUILD)/mcu_c/c_api

$(BUILD)/mpu:
	mkdir -p $(BUILD)/mpu

$(BUILD):
	mkdir -p $(BUILD)

cppcheck:
	$(CPPCHECK) --error-exitcode=1 -Iinclude $(CPPCHECK_DIRS)

clean:
	rm -rf $(BUILD)
