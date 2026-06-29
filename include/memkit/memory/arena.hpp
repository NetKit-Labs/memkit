#pragma once

#include "../config.hpp"
#include "../detail/utility.hpp"
#include "../status.hpp"
#include "fixed_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace memkit::memory {

template<typename BackingStorage>
class arena {
public:
    using backing_type = BackingStorage;

    arena() noexcept = default;

    explicit arena(BackingStorage backing) noexcept
        : backing_{std::move(backing)}
    {}

    [[nodiscard]] std::byte* base() noexcept
    {
        return backing_.data();
    }

    [[nodiscard]] const std::byte* base() const noexcept
    {
        return backing_.data();
    }

    [[nodiscard]] std::size_t capacity_bytes() const noexcept
    {
        return backing_.size();
    }

    [[nodiscard]] std::size_t used_bytes() const noexcept
    {
        return offset_bytes_;
    }

    [[nodiscard]] std::size_t remaining_bytes() const noexcept
    {
        if (offset_bytes_ > capacity_bytes()) {
            return 0u;
        }
        return capacity_bytes() - offset_bytes_;
    }

    [[nodiscard]] std::size_t allocation_count() const noexcept
    {
        return allocation_count_;
    }

    void reset() noexcept
    {
        offset_bytes_     = 0u;
        allocation_count_ = 0u;
    }

    [[nodiscard]] status allocate(std::size_t size, std::size_t alignment, void** out_ptr)
    {
        if (out_ptr == nullptr) {
            return status::null_ptr;
        }
        *out_ptr = nullptr;

        if (base() == nullptr || capacity_bytes() == 0u) {
            return status::null_ptr;
        }
        if (size == 0u || alignment == 0u || !detail::is_power_of_two(alignment)) {
            return status::invalid;
        }
        if (offset_bytes_ > capacity_bytes()) {
            return status::oom;
        }

        const std::uintptr_t base_addr = reinterpret_cast<std::uintptr_t>(base());
        const std::uintptr_t raw_addr  = base_addr + offset_bytes_;
        if (raw_addr < base_addr) {
            return status::oom;
        }

        const std::uintptr_t aligned_addr = detail::align_up_address(raw_addr, alignment);
        const std::size_t aligned_offset = static_cast<std::size_t>(aligned_addr - base_addr);
        if (aligned_offset < offset_bytes_) {
            return status::oom;
        }

        if (detail::add_would_overflow(aligned_offset, size)) {
            return status::oom;
        }
        const std::size_t new_offset = aligned_offset + size;

        if (new_offset > capacity_bytes()) {
            return status::oom;
        }

        *out_ptr = reinterpret_cast<void*>(aligned_addr);
        offset_bytes_ = new_offset;
        ++allocation_count_;
        return status::ok;
    }

    [[nodiscard]] status calloc(
        std::size_t count,
        std::size_t size,
        std::size_t alignment,
        void** out_ptr
    )
    {
        if (count != 0u && detail::mul_would_overflow(count, size)) {
            return status::invalid;
        }

        const std::size_t total = count * size;
        const status st = allocate(total, alignment, out_ptr);
        if (!ok(st)) {
            return st;
        }

        std::memset(*out_ptr, 0, total);
        return status::ok;
    }

    [[nodiscard]] BackingStorage&       backing()       noexcept { return backing_; }
    [[nodiscard]] const BackingStorage& backing() const noexcept { return backing_; }

private:
    BackingStorage backing_{};
    std::size_t    offset_bytes_     = 0u;
    std::size_t    allocation_count_ = 0u;
};

} // namespace memkit::memory
