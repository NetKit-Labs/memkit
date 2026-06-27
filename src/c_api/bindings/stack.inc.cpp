#include "stack.h"

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/stack_box.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

stack_status_t stack_init(cstack_t* stack, const stack_config_t* config)
{
    if (stack == NULL) {
        return STACK_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(stack, MEMKIT_STACK_OBJ_BYTES);

    return memkit::c_api::stack_box::from(stack).init(config);
}

stack_status_t stack_create(
    cstack_t** out_stack,
    size_t elem_size,
    size_t initial_capacity,
    arena_t* arena,
    unsigned flags
)
{
    if (out_stack == NULL) {
        return STACK_ERR_NULL;
    }
    if (elem_size == 0u) {
        return STACK_ERR_INVALID;
    }

    if (initial_capacity == 0u) {
        initial_capacity = 1u;
    }

    cstack_t* stack = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return STACK_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &stack)) {
        return arena == NULL ? STACK_ERR_OOM : STACK_ERR_INVALID;
    }

    stack_config_t config = {
        .elem_size = elem_size,
        .capacity  = initial_capacity,
        .arena     = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | STACK_FLAG_OWNS_STORAGE | STACK_FLAG_GROWABLE,
            arena,
            {
                STACK_FLAG_OWNS_STORAGE,
                STACK_FLAG_DYNAMIC_STORAGE,
                STACK_FLAG_ARENA_STORAGE,
                STACK_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const stack_status_t status = stack_init(stack, &config);
    if (!stack_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, stack);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::stack_box::from(stack).set_c_flags(
            memkit::c_api::stack_box::from(stack).c_flags() | STACK_FLAG_OWNS_SELF
        );
    }

    *out_stack = stack;
    return STACK_OK;
}

void stack_deinit(cstack_t* stack)
{
    if (stack == NULL) {
        return;
    }

    memkit::c_api::stack_box::from(stack).deinit();
    memkit::c_api::detail::zero_opaque_bytes(stack, MEMKIT_STACK_OBJ_BYTES);
}

void stack_destroy(cstack_t* stack)
{
    memkit::c_api::detail::destroy_owned_object<cstack_t, memkit::c_api::stack_box>(
        stack,
        STACK_FLAG_OWNS_SELF,
        STACK_FLAG_DYNAMIC_STORAGE,
        stack_deinit
    );
}

size_t stack_size(const cstack_t* stack)
{
    return stack != NULL ? memkit::c_api::stack_box::from(stack).core().size() : 0u;
}

size_t stack_capacity(const cstack_t* stack)
{
    return stack != NULL ? memkit::c_api::stack_box::from(stack).core().capacity() : 0u;
}

bool stack_empty(const cstack_t* stack)
{
    return stack == NULL || memkit::c_api::stack_box::from(stack).core().empty();
}

bool stack_full(const cstack_t* stack)
{
    return stack == NULL || memkit::c_api::stack_box::from(stack).full();
}

void stack_clear(cstack_t* stack)
{
    if (stack != NULL) {
        memkit::c_api::stack_box::from(stack).core().clear();
    }
}

stack_status_t stack_reserve(cstack_t* stack, size_t min_capacity)
{
    if (stack == NULL) {
        return STACK_ERR_NULL;
    }
    return static_cast<stack_status_t>(
        memkit::c_api::stack_box::from(stack).core().reserve(min_capacity)
    );
}

stack_status_t stack_push(cstack_t* stack, const void* elem)
{
    if (stack == NULL) {
        return STACK_ERR_NULL;
    }
    return static_cast<stack_status_t>(
        memkit::c_api::stack_box::from(stack).core().push_back(elem)
    );
}

stack_status_t stack_pop(cstack_t* stack, void* out_elem)
{
    if (stack == NULL) {
        return STACK_ERR_NULL;
    }
    return static_cast<stack_status_t>(
        memkit::c_api::stack_box::from(stack).core().pop_back(out_elem)
    );
}

stack_status_t stack_peek(const cstack_t* stack, void* out_elem)
{
    if (stack == NULL) {
        return STACK_ERR_NULL;
    }
    return static_cast<stack_status_t>(
        memkit::c_api::stack_box::from(stack).core().peek_back(out_elem)
    );
}

void* stack_top(cstack_t* stack)
{
    if (stack == NULL) {
        return NULL;
    }
    auto& core = memkit::c_api::stack_box::from(stack).core();
    return core.empty() ? NULL : core.at(core.size() - 1u);
}

const void* stack_top_const(const cstack_t* stack)
{
    if (stack == NULL) {
        return NULL;
    }
    const auto& core = memkit::c_api::stack_box::from(stack).core();
    return core.empty() ? NULL : core.at(core.size() - 1u);
}

stack_status_t stack_foreach(const cstack_t* stack, stack_visit_fn visit, void* user)
{
    if (stack == NULL || visit == NULL) {
        return STACK_ERR_NULL;
    }

    return static_cast<stack_status_t>(
        memkit::c_api::stack_box::from(stack).core().foreach(
            [visit, user](const void* elem, size_t index) -> memkit::status {
                return static_cast<memkit::status>(visit(elem, index, user));
            }
        )
    );
}

} // extern "C"
