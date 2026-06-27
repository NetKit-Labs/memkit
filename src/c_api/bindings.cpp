/*
 * Single translation unit for all C API extern "C" bindings.
 * Implementation logic lives in include/memkit/c_api/ (box headers).
 */

#include <memkit/c_api/static_checks.hpp>

namespace {
const bool memkit_c_api_layout_ok = ([] {
    memkit::c_api::detail::assert_c_api_object_layout();
    return true;
}());
} // namespace

#include "bindings/bitset.inc.cpp"
#include "bindings/btree.inc.cpp"
#include "bindings/deque.inc.cpp"
#include "bindings/dlist.inc.cpp"
#include "bindings/handle_pool.inc.cpp"
#include "bindings/hashmap.inc.cpp"
#include "bindings/list.inc.cpp"
#include "bindings/lrucache.inc.cpp"
#include "bindings/objpool.inc.cpp"
#include "bindings/pqueue.inc.cpp"
#include "bindings/queue.inc.cpp"
#include "bindings/ring.inc.cpp"
#include "bindings/stack.inc.cpp"
#include "bindings/vector.inc.cpp"
