#pragma once

#include "../config.hpp"

#include <cstddef>
#include <cstdint>

namespace memkit::memory {

#if MEMKIT_ALLOW_MMAP

/*
 * Owning virtual-memory-backed byte storage for MPU arena backing (optional).
 * POSIX: anonymous mmap/munmap. Windows: VirtualAlloc/VirtualFree.
 * Implementation lives in src/mmap_backing.cpp.
 */
class mmap_storage {
public:
    mmap_storage() noexcept = default;

    mmap_storage(const mmap_storage&)            = delete;
    mmap_storage& operator=(const mmap_storage&) = delete;

    mmap_storage(mmap_storage&& other) noexcept;
    mmap_storage& operator=(mmap_storage&& other) noexcept;

    ~mmap_storage();

    [[nodiscard]] static mmap_storage map(std::size_t bytes);

    /* Release a detached or arena-owned mapping (platform unmap/free). */
    static void unmap(std::byte* base, std::size_t bytes) noexcept;

    [[nodiscard]] std::byte*       data()       noexcept { return base_; }
    [[nodiscard]] const std::byte* data() const noexcept { return base_; }
    [[nodiscard]] std::size_t        size() const noexcept { return bytes_; }
    [[nodiscard]] bool               valid() const noexcept { return base_ != nullptr; }

    void release() noexcept;

    /* Transfer ownership without unmapping; caller must unmap via unmap() or release(). */
    [[nodiscard]] std::byte* detach() noexcept;

private:
    std::byte*  base_  = nullptr;
    std::size_t bytes_ = 0u;
};

#endif // MEMKIT_ALLOW_MMAP

} // namespace memkit::memory
