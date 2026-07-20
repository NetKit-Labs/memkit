#include "memkit/memory/mmap.hpp"

#if MEMKIT_ALLOW_MMAP

#include <cstdlib>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace memkit::memory {
namespace {

[[nodiscard]] std::size_t mapped_page_size() noexcept
{
#if defined(_WIN32)
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<std::size_t>(info.dwPageSize);
#else
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return 0u;
    }
    return static_cast<std::size_t>(page_size);
#endif
}

[[nodiscard]] std::size_t round_up_to_page(std::size_t bytes, std::size_t page) noexcept
{
    return ((bytes + page - 1u) / page) * page;
}

} // namespace

mmap_storage::mmap_storage(mmap_storage&& other) noexcept
    : base_{other.base_}
    , bytes_{other.bytes_}
{
    other.base_  = nullptr;
    other.bytes_ = 0u;
}

mmap_storage& mmap_storage::operator=(mmap_storage&& other) noexcept
{
    if (this != &other) {
        release();
        base_        = other.base_;
        bytes_       = other.bytes_;
        other.base_  = nullptr;
        other.bytes_ = 0u;
    }
    return *this;
}

mmap_storage::~mmap_storage()
{
    release();
}

mmap_storage mmap_storage::map(std::size_t bytes)
{
    mmap_storage storage;
    if (bytes == 0u) {
        return storage;
    }

    const std::size_t page = mapped_page_size();
    if (page == 0u) {
        return storage;
    }

    const std::size_t mapped_bytes = round_up_to_page(bytes, page);

#if defined(_WIN32)
    void* const ptr = VirtualAlloc(
        nullptr,
        mapped_bytes,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (ptr == nullptr) {
        return storage;
    }
#else
    void* const ptr = mmap(
        nullptr,
        mapped_bytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (ptr == MAP_FAILED) {
        return storage;
    }
#endif

    storage.base_  = static_cast<std::byte*>(ptr);
    storage.bytes_ = mapped_bytes;
    return storage;
}

void mmap_storage::unmap(std::byte* base, std::size_t bytes) noexcept
{
    if (base == nullptr) {
        return;
    }

#if defined(_WIN32)
    (void)bytes;
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, bytes);
#endif
}

void mmap_storage::release() noexcept
{
    if (base_ == nullptr) {
        return;
    }

    unmap(base_, bytes_);
    base_  = nullptr;
    bytes_ = 0u;
}

std::byte* mmap_storage::detach() noexcept
{
    std::byte* const ptr = base_;
    base_  = nullptr;
    bytes_ = 0u;
    return ptr;
}

} // namespace memkit::memory

#endif // MEMKIT_ALLOW_MMAP
