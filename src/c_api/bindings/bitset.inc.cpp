#include "bitset.h"

#include <memkit/c_api/create_object.hpp>
#include <memkit/c_api/object_lifecycle.hpp>
#include <memkit/c_api/bitset_box.hpp>
#include <memkit/c_api/status_cast.hpp>
#include <memkit/detail/bitset_core.hpp>

#if MEMKIT_ALLOW_HEAP
#include <cstdlib>
#endif

#include <cstddef>

extern "C" {

size_t bitset_storage_bytes(size_t capacity)
{
    return memkit::detail::bitset_core::storage_bytes(capacity);
}

uint8_t bitset_tail_mask(size_t capacity)
{
    return memkit::detail::bitset_core::tail_mask(capacity);
}

bitset_status_t bitset_init(bitset_t *bitset, const bitset_config_t *config)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }

    memkit::c_api::detail::zero_opaque_bytes(bitset, MEMKIT_BITSET_OBJ_BYTES);

    return memkit::c_api::bitset_box::from(bitset).init(config);
}

bitset_status_t bitset_create(
    bitset_t **out_bitset,
    size_t capacity,
    arena_t *arena,
    unsigned flags
)
{
    if (out_bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    if (capacity == 0u) {
        return BITSET_ERR_INVALID;
    }

    bitset_t *bitset = NULL;

#if !MEMKIT_ALLOW_HEAP
    if (arena == NULL) {
        return BITSET_ERR_UNSUPPORTED;
    }
#endif

    if (!memkit::c_api::detail::allocate_object(arena, &bitset)) {
        return arena == NULL ? BITSET_ERR_OOM : BITSET_ERR_INVALID;
    }

    bitset_config_t config = {
        .capacity = capacity,
        .arena = arena,
        .flags     = memkit::c_api::detail::owned_create_config_flags(
            flags | BITSET_FLAG_OWNS_STORAGE,
            arena,
            {
                BITSET_FLAG_OWNS_STORAGE,
                BITSET_FLAG_DYNAMIC_STORAGE,
                BITSET_FLAG_ARENA_STORAGE,
                BITSET_FLAG_OWNS_SELF,
            },
            0u
        ),
    };

    const bitset_status_t status = bitset_init(bitset, &config);
    if (!bitset_status_ok(status)) {
        memkit::c_api::detail::release_uninitialized_object(arena, bitset);
        return status;
    }

#if MEMKIT_ALLOW_HEAP
    if (arena != NULL)
#endif
    {
        memkit::c_api::bitset_box::from(bitset).set_c_flags(
            memkit::c_api::bitset_box::from(bitset).c_flags() | BITSET_FLAG_OWNS_SELF
        );
    }

    *out_bitset = bitset;
    return BITSET_OK;
}

void bitset_deinit(bitset_t *bitset)
{
    if (bitset == NULL) {
        return;
    }

    memkit::c_api::bitset_box::from(bitset).deinit();
    memkit::c_api::detail::zero_opaque_bytes(bitset, MEMKIT_BITSET_OBJ_BYTES);
}

void bitset_destroy(bitset_t *bitset)
{
    memkit::c_api::detail::destroy_owned_object<bitset_t, memkit::c_api::bitset_box>(
        bitset,
        BITSET_FLAG_OWNS_SELF,
        BITSET_FLAG_DYNAMIC_STORAGE,
        bitset_deinit
    );
}

size_t bitset_capacity(const bitset_t *bitset)
{
    return bitset != NULL ? memkit::c_api::bitset_box::from(bitset).core().capacity() : 0u;
}

size_t bitset_size(const bitset_t *bitset)
{
    return bitset != NULL ? memkit::c_api::bitset_box::from(bitset).core().size() : 0u;
}

bool bitset_empty(const bitset_t *bitset)
{
    return bitset == NULL || memkit::c_api::bitset_box::from(bitset).core().empty();
}

bool bitset_full(const bitset_t *bitset)
{
    return bitset != NULL && memkit::c_api::bitset_box::from(bitset).core().full();
}

void bitset_clear(bitset_t *bitset)
{
    if (bitset != NULL) {
        memkit::c_api::bitset_box::from(bitset).core().clear();
    }
}

bitset_status_t bitset_set_all(bitset_t *bitset)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().set_all()
    );
}

bool bitset_test(const bitset_t *bitset, size_t index)
{
    return bitset != NULL && memkit::c_api::bitset_box::from(bitset).core().test(index);
}

bitset_status_t bitset_set(bitset_t *bitset, size_t index)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().set(index)
    );
}

bitset_status_t bitset_reset(bitset_t *bitset, size_t index)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().reset(index)
    );
}

bitset_status_t bitset_toggle(bitset_t *bitset, size_t index)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().toggle(index)
    );
}

bitset_status_t bitset_assign(bitset_t *bitset, bool value, size_t index)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().assign(value, index)
    );
}

bitset_status_t bitset_find_first_set(
    const bitset_t *bitset,
    size_t start_index,
    size_t *out_index
)
{
    if (bitset == NULL || out_index == NULL) {
        return BITSET_ERR_NULL;
    }

    std::size_t found = 0u;
    const memkit::status st =
        memkit::c_api::bitset_box::from(bitset).core().find_first_set(start_index, found);
    if (st == memkit::status::ok) {
        *out_index = found;
    }
    return memkit::c_api::to_bitset_status(st);
}

bitset_status_t bitset_find_first_clear(
    const bitset_t *bitset,
    size_t start_index,
    size_t *out_index
)
{
    if (bitset == NULL || out_index == NULL) {
        return BITSET_ERR_NULL;
    }

    std::size_t found = 0u;
    const memkit::status st =
        memkit::c_api::bitset_box::from(bitset).core().find_first_clear(start_index, found);
    if (st == memkit::status::ok) {
        *out_index = found;
    }
    return memkit::c_api::to_bitset_status(st);
}

bitset_status_t bitset_copy(bitset_t *dst, const bitset_t *src)
{
    if (dst == NULL || src == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(dst).core().copy_from(
            memkit::c_api::bitset_box::from(src).core()
        )
    );
}

bool bitset_equal(const bitset_t *left, const bitset_t *right)
{
    return left != NULL && right != NULL &&
           memkit::c_api::bitset_box::from(left).core().equal(
               memkit::c_api::bitset_box::from(right).core()
           );
}

bitset_status_t bitset_union_with(bitset_t *bitset, const bitset_t *other)
{
    if (bitset == NULL || other == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().union_with(
            memkit::c_api::bitset_box::from(other).core()
        )
    );
}

bitset_status_t bitset_intersect_with(bitset_t *bitset, const bitset_t *other)
{
    if (bitset == NULL || other == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().intersect_with(
            memkit::c_api::bitset_box::from(other).core()
        )
    );
}

bitset_status_t bitset_xor_with(bitset_t *bitset, const bitset_t *other)
{
    if (bitset == NULL || other == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().xor_with(
            memkit::c_api::bitset_box::from(other).core()
        )
    );
}

bitset_status_t bitset_complement(bitset_t *bitset)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().complement()
    );
}

bitset_status_t bitset_load_bytes(bitset_t *bitset, const void *bytes, size_t bytes_len)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().load_bytes(
            static_cast<const std::byte *>(bytes),
            bytes_len
        )
    );
}

bitset_status_t bitset_store_bytes(const bitset_t *bitset, void *bytes, size_t bytes_len)
{
    if (bitset == NULL) {
        return BITSET_ERR_NULL;
    }
    return memkit::c_api::to_bitset_status(
        memkit::c_api::bitset_box::from(bitset).core().store_bytes(
            static_cast<std::byte *>(bytes),
            bytes_len
        )
    );
}

uint8_t *bitset_data(bitset_t *bitset)
{
    return bitset != NULL ? memkit::c_api::bitset_box::from(bitset).core().data() : NULL;
}

const uint8_t *bitset_data_const(const bitset_t *bitset)
{
    return bitset != NULL ? memkit::c_api::bitset_box::from(bitset).core().data() : NULL;
}

size_t bitset_data_bytes(const bitset_t *bitset)
{
    return bitset != NULL ?
        memkit::c_api::bitset_box::from(bitset).core().storage_byte_count() : 0u;
}

bitset_status_t bitset_foreach(
    const bitset_t *bitset,
    bitset_visit_fn visit,
    void *user
)
{
    if (bitset == NULL || visit == NULL) {
        return BITSET_ERR_NULL;
    }

    for (std::size_t index = 0u; index < memkit::c_api::bitset_box::from(bitset).core().capacity(); ++index) {
        if (!memkit::c_api::bitset_box::from(bitset).core().test(index)) {
            continue;
        }

        const bitset_status_t status = visit(index, user);
        if (!bitset_status_ok(status)) {
            return status;
        }
    }

    return BITSET_OK;
}

} // extern "C"
