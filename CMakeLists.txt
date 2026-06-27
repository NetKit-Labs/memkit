cmake_minimum_required(VERSION 3.25)
project(memkit LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MEMKIT_EMBEDDED_LINUX "Build for MPU (embedded Linux): heap + optional mmap" OFF)
option(MEMKIT_ALLOW_MMAP "Enable mmap-backed arena on MPU" ON)
option(MEMKIT_USE_STL "MPU only: expose std::vector/string aliases in memkit/stl.hpp" OFF)

if(MEMKIT_EMBEDDED_LINUX)
    add_compile_definitions(MEMKIT_MPU=1 EMBEDDED_LINUX=1)
else()
    add_compile_definitions(MEMKIT_MCU=1)
endif()

if(MEMKIT_ALLOW_MMAP AND MEMKIT_EMBEDDED_LINUX)
    add_compile_definitions(MEMKIT_ALLOW_MMAP=1)
endif()

if(MEMKIT_USE_STL AND MEMKIT_EMBEDDED_LINUX)
    add_compile_definitions(MEMKIT_USE_STL=1)
endif()

add_library(memkit STATIC
    src/arena.cpp
    src/mmap_backing.cpp
    src/c_api/bindings.cpp
)

target_include_directories(memkit PUBLIC include)

if(MEMKIT_EMBEDDED_LINUX)
    target_compile_definitions(memkit PUBLIC MEMKIT_MPU=1 EMBEDDED_LINUX=1)
else()
    target_compile_definitions(memkit PUBLIC MEMKIT_MCU=1)
endif()

add_executable(example_mcu examples/example_mcu.cpp)
target_link_libraries(example_mcu PRIVATE memkit)

add_executable(example_embedded_patterns examples/example_embedded_patterns.cpp)
target_link_libraries(example_embedded_patterns PRIVATE memkit)

add_executable(example_comm_pipeline examples/example_comm_pipeline.cpp)
target_link_libraries(example_comm_pipeline PRIVATE memkit)

add_executable(example_mcu_c examples/example_mcu_c.c)
target_link_libraries(example_mcu_c PRIVATE memkit)

if(MEMKIT_EMBEDDED_LINUX)
    add_executable(example_mpu examples/example_mpu.cpp)
    target_link_libraries(example_mpu PRIVATE memkit)

    add_executable(example_mpu_c examples/example_mpu.c)
    target_link_libraries(example_mpu_c PRIVATE memkit)

    set(MEMKIT_C_API_MPU_TESTS
        test_deque_c test_hashmap_c test_btree_c test_pqueue_c test_list_c
        test_dlist_c test_lrucache_c
    )

    foreach(test_name IN LISTS MEMKIT_C_API_MPU_TESTS)
        add_executable(${test_name} tests/${test_name}.c)
        target_link_libraries(${test_name} PRIVATE memkit)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    add_executable(test_c_api_extended tests/test_c_api_extended.c)
    target_link_libraries(test_c_api_extended PRIVATE memkit)
    add_test(NAME test_c_api_extended COMMAND test_c_api_extended)
endif()

enable_testing()

set(MEMKIT_C_API_MCU_TESTS
    test_arena_c test_ring_c test_queue_c test_vector_c test_stack_c
    test_bitset_c test_objpool_c test_handle_pool_c
)

foreach(test_name IN LISTS MEMKIT_C_API_MCU_TESTS)
    add_executable(${test_name} tests/${test_name}.c)
    target_link_libraries(${test_name} PRIVATE memkit)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

set(MEMKIT_CPP_TESTS
    test_ring_cpp test_vector_cpp test_hashmap_cpp test_list_cpp test_dlist_cpp
    test_btree_cpp test_stack_queue_cpp test_pqueue_cpp test_objpool_cpp
    test_bitset_cpp test_deque_cpp test_lrucache_cpp test_small_string_cpp test_byte_ring_cpp
    test_handle_pool_cpp test_intrusive_list_cpp test_spsc_queue_cpp test_flat_map_cpp
    test_timer_wheel_cpp test_double_buffer_cpp test_mpsc_queue_cpp test_enum_map_cpp
    test_ring_log_cpp test_sparse_set_cpp test_small_buffer_cpp test_fixed_variant_cpp
    test_token_bucket_cpp test_fixed_iovec_cpp test_lookup_table_cpp
    test_bit_stream_cpp test_running_stats_cpp
)

foreach(test_name IN LISTS MEMKIT_CPP_TESTS)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE memkit)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
