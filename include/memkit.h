#ifndef MEMKIT_H
#define MEMKIT_H

/*
 * memkit C23 API umbrella header.
 *
 * C API tiers (see memkit_config.h):
 *   Tier 1 — arena, ring, vector, stack, queue, bitset, objpool, handle_pool (MCU + MPU)
 *   Tier 2 — hashmap, btree, pqueue, list, dlist, lrucache, deque (MPU only;
 *            MCU builds stub init/create with *_ERR_UNSUPPORTED)
 *
 * C++ users: include <memkit/memkit.hpp> for all containers on any target.
 * C users on MCU firmware: tier-1 C API only; use memkit.hpp for tier-2.
 */

#include "memkit_config.h"
#include "arena.h"
#include "ring.h"
#include "vector.h"
#include "stack.h"
#include "queue.h"
#include "deque.h"
#include "pqueue.h"
#include "list.h"
#include "dlist.h"
#include "btree.h"
#include "hashmap.h"
#include "lrucache.h"
#include "objpool.h"
#include "bitset.h"
#include "handle_pool.h"
#include "memkit_helpers.h"

#endif /* MEMKIT_H */
