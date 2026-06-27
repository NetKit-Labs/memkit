#pragma once

/*
 * Opaque object sizes for C API containers.
 * Values are verified at library build time in src/c_api/bindings.cpp.
 */
#ifndef MEMKIT_OBJECT_SIZES_H
#define MEMKIT_OBJECT_SIZES_H

#include <stddef.h>

#define MEMKIT_RING_OBJ_BYTES     160u
#define MEMKIT_VECTOR_OBJ_BYTES   192u
#define MEMKIT_STACK_OBJ_BYTES    192u
#define MEMKIT_QUEUE_OBJ_BYTES    192u
#define MEMKIT_DEQUE_OBJ_BYTES    256u
#define MEMKIT_LIST_OBJ_BYTES     192u
#define MEMKIT_DLIST_OBJ_BYTES    256u
#define MEMKIT_BITSET_OBJ_BYTES   128u
#define MEMKIT_OBJPOOL_OBJ_BYTES  192u
#define MEMKIT_PQUEUE_OBJ_BYTES   256u
#define MEMKIT_HASHMAP_OBJ_BYTES  512u
#define MEMKIT_BTREE_OBJ_BYTES    512u
#define MEMKIT_LRUCACHE_OBJ_BYTES 512u
#define MEMKIT_HANDLE_POOL_OBJ_BYTES 128u

#endif /* MEMKIT_OBJECT_SIZES_H */
