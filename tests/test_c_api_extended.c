/*
 * MPU integration test: mmap-backed arena_create and every *_create helper
 * on a single shared arena. Per-container behavior is covered by test_*_c.c.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "bitset.h"
#include "btree.h"
#include "deque.h"
#include "dlist.h"
#include "handle_pool.h"
#include "hashmap.h"
#include "list.h"
#include "lrucache.h"
#include "objpool.h"
#include "pqueue.h"
#include "queue.h"
#include "ring.h"
#include "stack.h"
#include "vector.h"

#if MEMKIT_C_API_EXTENDED

static int compare_u32(const void *a, const void *b, void *user)
{
    (void)user;
    const uint32_t av = *(const uint32_t *)a;
    const uint32_t bv = *(const uint32_t *)b;
    if (av < bv) {
        return -1;
    }
    if (av > bv) {
        return 1;
    }
    return 0;
}

int main(void)
{
    arena_t *arena = NULL;
    assert(arena_status_ok(arena_create(&arena, 262144u)));

    const uint32_t sample = 7u;
    const int32_t sample_i32 = 55;

    ring_t *ring = NULL;
    assert(ring_status_ok(ring_create(&ring, sizeof(uint32_t), 4u, arena, 0u)));
    assert(ring_status_ok(ring_push_back(ring, &sample)));
    ring_destroy(ring);

    queue_t *queue = NULL;
    assert(queue_status_ok(queue_create(&queue, sizeof(uint32_t), 4u, arena, 0u)));
    assert(queue_status_ok(queue_push(queue, &sample)));
    queue_destroy(queue);

    cstack_t *stack = NULL;
    assert(stack_status_ok(stack_create(&stack, sizeof(int32_t), 4u, arena, 0u)));
    assert(stack_status_ok(stack_push(stack, &sample_i32)));
    stack_destroy(stack);

    vector_t *vector = NULL;
    assert(vector_status_ok(vector_create(&vector, sizeof(int32_t), 4u, arena, 0u)));
    assert(vector_status_ok(vector_push_back(vector, &sample_i32)));
    vector_destroy(vector);

    bitset_t *bitset = NULL;
    assert(bitset_status_ok(bitset_create(&bitset, 32u, arena, 0u)));
    assert(bitset_status_ok(bitset_set(bitset, 3u)));
    bitset_destroy(bitset);

    objpool_t *objpool = NULL;
    assert(objpool_status_ok(objpool_create(&objpool, sizeof(uint32_t), 4u, arena, 0u)));
    void *pool_slot = NULL;
    assert(objpool_status_ok(objpool_alloc(objpool, &pool_slot)));
    objpool_destroy(objpool);

    handle_pool_t *handles = NULL;
    assert(handle_pool_status_ok(handle_pool_create(
        &handles, sizeof(int32_t), 4u, arena, 0u)));
    void *handle_slot = NULL;
    handle_t handle = HANDLE_POOL_INVALID_HANDLE;
    assert(handle_pool_status_ok(handle_pool_acquire(handles, &handle_slot, &handle)));
    handle_pool_destroy(handles);

    hashmap_t *map = NULL;
    assert(hashmap_status_ok(hashmap_create(
        &map,
        sizeof(uint32_t),
        sizeof(int32_t),
        8u,
        HASHMAP_STRATEGY_CHAINING,
        arena,
        HASHMAP_FLAG_GROWABLE
    )));
    const int32_t mapped = 42;
    assert(hashmap_status_ok(hashmap_put(map, &sample, &mapped)));
    hashmap_destroy(map);

    list_t *list = NULL;
    assert(list_status_ok(list_create(&list, sizeof(uint32_t), arena, 0u)));
    assert(list_status_ok(list_push_back(list, &sample)));
    list_destroy(list);

    dlist_t *dlist = NULL;
    assert(dlist_status_ok(dlist_create(&dlist, sizeof(uint32_t), arena, 0u)));
    assert(dlist_status_ok(dlist_push_back(dlist, &sample)));
    dlist_destroy(dlist);

    deque_t *deque = NULL;
    assert(deque_status_ok(deque_create(&deque, sizeof(uint32_t), 4u, arena, 0u)));
    assert(deque_status_ok(deque_push_back(deque, &sample)));
    deque_destroy(deque);

    pqueue_t *pqueue = NULL;
    assert(pqueue_status_ok(pqueue_create(
        &pqueue,
        sizeof(uint32_t),
        compare_u32,
        8u,
        arena,
        PQUEUE_FLAG_GROWABLE
    )));
    const uint32_t pq_values[] = {5u, 1u, 9u, 3u};
    for (size_t i = 0u; i < 4u; ++i) {
        assert(pqueue_status_ok(pqueue_push(pqueue, &pq_values[i])));
    }
    uint32_t pq_top = 0u;
    assert(pqueue_status_ok(pqueue_peek(pqueue, &pq_top)));
    assert(pq_top == 1u);
    pqueue_destroy(pqueue);

    btree_t *tree = NULL;
    assert(btree_status_ok(btree_create(
        &tree,
        sizeof(uint32_t),
        compare_u32,
        arena,
        0u
    )));
    assert(btree_status_ok(btree_insert(tree, &sample)));
    btree_destroy(tree);

    lrucache_t *cache = NULL;
    assert(lrucache_status_ok(lrucache_create(
        &cache,
        sizeof(uint32_t),
        sizeof(int32_t),
        4u,
        0u,
        arena,
        0u
    )));
    const int32_t cached_value = 3;
    assert(lrucache_status_ok(lrucache_put(cache, &sample, &cached_value)));
    lrucache_destroy(cache);

    arena_destroy(arena);

    printf("c_api_integration: ok\n");
    return 0;
}

#else

int main(void)
{
    return 0;
}

#endif
