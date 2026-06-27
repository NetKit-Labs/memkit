#pragma once

#include <memkit/c_api/bitset_box.hpp>
#include <memkit/c_api/deque_box.hpp>
#include <memkit/c_api/handle_pool_box.hpp>
#include <memkit/c_api/objpool_box.hpp>
#include <memkit/c_api/queue_box.hpp>
#include <memkit/c_api/ring_box.hpp>
#include <memkit/c_api/stack_box.hpp>
#include <memkit/c_api/vector_box.hpp>
#include <memkit_object_sizes.h>

#include <bitset.h>
#include <deque.h>
#include <handle_pool.h>
#include <objpool.h>
#include <queue.h>
#include <ring.h>
#include <stack.h>
#include <vector.h>

#if MEMKIT_C_API_EXTENDED
#include <memkit/c_api/btree_box.hpp>
#include <memkit/c_api/dlist_box.hpp>
#include <memkit/c_api/hashmap_box.hpp>
#include <memkit/c_api/list_box.hpp>
#include <memkit/c_api/lrucache_box.hpp>
#include <memkit/c_api/pqueue_box.hpp>

#include <btree.h>
#include <dlist.h>
#include <hashmap.h>
#include <list.h>
#include <lrucache.h>
#include <pqueue.h>
#endif

namespace memkit::c_api::detail {

inline void assert_c_api_object_layout() noexcept
{
    static_assert(sizeof(ring_box) <= MEMKIT_RING_OBJ_BYTES);
    static_assert(alignof(ring_box) <= alignof(ring_t));

    static_assert(sizeof(vector_box) <= MEMKIT_VECTOR_OBJ_BYTES);
    static_assert(alignof(vector_box) <= alignof(vector_t));

    static_assert(sizeof(stack_box) <= MEMKIT_STACK_OBJ_BYTES);
    static_assert(alignof(stack_box) <= alignof(cstack_t));

    static_assert(sizeof(queue_box) <= MEMKIT_QUEUE_OBJ_BYTES);
    static_assert(alignof(queue_box) <= alignof(queue_t));

    static_assert(sizeof(handle_pool_box) <= MEMKIT_HANDLE_POOL_OBJ_BYTES);
    static_assert(alignof(handle_pool_box) <= alignof(handle_pool_t));

    static_assert(sizeof(bitset_box) <= MEMKIT_BITSET_OBJ_BYTES);
    static_assert(alignof(bitset_box) <= alignof(bitset_t));

    static_assert(sizeof(objpool_box) <= MEMKIT_OBJPOOL_OBJ_BYTES);
    static_assert(alignof(objpool_box) <= alignof(objpool_t));

#if MEMKIT_C_API_EXTENDED
    static_assert(sizeof(deque_box) <= MEMKIT_DEQUE_OBJ_BYTES);
    static_assert(alignof(deque_box) <= alignof(deque_t));

    static_assert(sizeof(pqueue_box) <= MEMKIT_PQUEUE_OBJ_BYTES);
    static_assert(alignof(pqueue_box) <= alignof(pqueue_t));

    static_assert(sizeof(list_box) <= MEMKIT_LIST_OBJ_BYTES);
    static_assert(alignof(list_box) <= alignof(list_t));

    static_assert(sizeof(dlist_box) <= MEMKIT_DLIST_OBJ_BYTES);
    static_assert(alignof(dlist_box) <= alignof(dlist_t));

    static_assert(sizeof(hashmap_box) <= MEMKIT_HASHMAP_OBJ_BYTES);
    static_assert(alignof(hashmap_box) <= alignof(hashmap_t));

    static_assert(sizeof(btree_box) <= MEMKIT_BTREE_OBJ_BYTES);
    static_assert(alignof(btree_box) <= alignof(btree_t));

    static_assert(sizeof(lrucache_box) <= MEMKIT_LRUCACHE_OBJ_BYTES);
    static_assert(alignof(lrucache_box) <= alignof(lrucache_t));
#endif
}

} // namespace memkit::c_api::detail
