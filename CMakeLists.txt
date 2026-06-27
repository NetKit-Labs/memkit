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

if(MEMKIT_EMBEDDED_LINUX)
    add_executable(example_mpu examples/example_mpu.cpp)
    target_link_libraries(example_mpu PRIVATE memkit)

    add_executable(example_mpu_c examples/example_mpu.c)
    target_link_libraries(example_mpu_c PRIVATE memkit)

    add_executable(test_c_api_extended tests/test_c_api_extended.c)
    target_link_libraries(test_c_api_extended PRIVATE memkit)
    add_test(NAME test_c_api_extended COMMAND test_c_api_extended)
endif()

enable_testing()

set(MEMKIT_CPP_TESTS
    test_ring_cpp test_vector_cpp test_hashmap_cpp test_list_cpp test_dlist_cpp
    test_btree_cpp test_stack_queue_cpp test_pqueue_cpp test_objpool_cpp
    test_bitset_cpp test_deque_cpp test_lrucache_cpp test_small_string_cpp test_byte_ring_cpp
    test_handle_pool_cpp
)

foreach(test_name IN LISTS MEMKIT_CPP_TESTS)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE memkit)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_executable(test_c_api_smoke tests/test_c_api_smoke.c)
target_link_libraries(test_c_api_smoke PRIVATE memkit)
add_test(NAME test_c_api_smoke COMMAND test_c_api_smoke)
