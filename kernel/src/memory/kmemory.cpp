#include "kmemory.h"
#include <paging/page_frame_allocator.h>

// Optimized version
void memcpy(void* dest, const void* src, size_t size) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    // Handle initial bytes until d is 8-byte aligned
    while (size && (reinterpret_cast<size_t>(d) & 0x7)) {
        *d++ = *s++;
        --size;
    }

    // Handle initial bytes until s is 8-byte aligned
    while (size && (reinterpret_cast<size_t>(s) & 0x7)) {
        *d++ = *s++;
        --size;
    }

    // Use 64-bit loads and stores for as long as possible
    uint64_t* d64 = reinterpret_cast<uint64_t*>(d);
    const uint64_t* s64 = reinterpret_cast<const uint64_t*>(s);

    while (size >= 8) {
        *d64++ = *s64++;
        size -= 8;
    }

    // Handle remaining bytes
    d = reinterpret_cast<uint8_t*>(d64);
    s = reinterpret_cast<const uint8_t*>(s64);
    
    while (size--) {
        *d++ = *s++;
    }
}

void memmove(void* dest, const void* src, size_t size) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    // If the buffers do not overlap, or the destination is in front, use memcpy logic
    if (d < s || d >= s + size) {
        memcpy(dest, src, size);
        return;
    }

    // If the destination is behind the source, we need to copy in reverse
    // Start from the end of the buffers

    // Handle initial bytes until d + size is 8-byte aligned
    while (size && (reinterpret_cast<size_t>(d + size - 1) & 0x7)) {
        size--;
        d[size] = s[size];
    }

    // Handle initial bytes until s + size is 8-byte aligned
    while (size && (reinterpret_cast<size_t>(s + size - 1) & 0x7)) {
        size--;
        d[size] = s[size];
    }

    // Use 64-bit loads and stores for as long as possible
    uint64_t* d64 = reinterpret_cast<uint64_t*>(d + size);
    const uint64_t* s64 = reinterpret_cast<const uint64_t*>(s + size);

    while (size >= 8) {
        d64--;
        s64--;
        size -= 8;
        *d64 = *s64;
    }

    // Handle remaining bytes in reverse
    d = reinterpret_cast<uint8_t*>(d64);
    s = reinterpret_cast<const uint8_t*>(s64);

    while (size--) {
        d--;
        s--;
        *d = *s;
    }
}

int memcmp(void* dest, void* src, size_t size) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    uint8_t* s = static_cast<uint8_t*>(src);

    // Handle initial bytes until d and s are 8-byte aligned
    while (size && (reinterpret_cast<size_t>(d) & 0x7)) {
        if (*d != *s) {
            return *d - *s;
        }
        ++d;
        ++s;
        --size;
    }

    // Use 64-bit comparisons for as long as possible
    uint64_t* d64 = reinterpret_cast<uint64_t*>(d);
    uint64_t* s64 = reinterpret_cast<uint64_t*>(s);

    while (size >= 8) {
        if (*d64 != *s64) {
            d = reinterpret_cast<uint8_t*>(d64);
            s = reinterpret_cast<uint8_t*>(s64);
            for (int i = 0; i < 8; ++i) {
                if (d[i] != s[i]) {
                    return d[i] - s[i];
                }
            }
        }
        ++d64;
        ++s64;
        size -= 8;
    }

    // Handle remaining bytes
    d = reinterpret_cast<uint8_t*>(d64);
    s = reinterpret_cast<uint8_t*>(s64);

    while (size--) {
        if (*d != *s) {
            return *d - *s;
        }
        ++d;
        ++s;
    }

    return 0;
}

// Optimized version
void memset(void* vaddr, uint8_t val, size_t size) {
    uint8_t* dst = static_cast<uint8_t*>(vaddr);

    // Handle small sizes separately
    while (size && ((reinterpret_cast<size_t>(dst) & 0x7) != 0)) {
        *dst = val;
        ++dst;
        --size;
    }

    // Prepare a 64-bit pattern
    uint64_t pattern = val;
    for (int i = 1; i < 8; ++i) {
        pattern = (pattern << 8) | val;
    }

    // Use 64-bit stores for as long as possible
    uint64_t* dst64 = reinterpret_cast<uint64_t*>(dst);
    while (size >= 8) {
        *dst64 = pattern;
        ++dst64;
        size -= 8;
    }

    // Handle remaining bytes
    dst = reinterpret_cast<uint8_t*>(dst64);
    while (size) {
        *dst = val;
        ++dst;
        --size;
    }
}

void zeromem(void* vaddr, size_t size) {
    memset(vaddr, 0, size);
}

void* allocPage() {
    auto& allocator = paging::getGlobalPageFrameAllocator();
    return allocator.requestFreePage();
}

void* zallocPage() {
    auto& allocator = paging::getGlobalPageFrameAllocator();
    return allocator.requestFreePageZeroed();
}

void* allocPages(size_t pages) {
    auto& allocator = paging::getGlobalPageFrameAllocator();
    return allocator.requestFreePages(pages);
}

void* zallocPages(size_t pages) {
    auto& allocator = paging::getGlobalPageFrameAllocator();
    return allocator.requestFreePagesZeroed(pages);
}

void* kmalloc(size_t size) {
    auto& heapAllocator = DynamicMemoryAllocator::get();
    return heapAllocator.allocate(size);
}

void kfree(void* ptr) {
    auto& heapAllocator = DynamicMemoryAllocator::get();
    heapAllocator.free(ptr);
}

void* krealloc(void* ptr, size_t size) {
    auto& heapAllocator = DynamicMemoryAllocator::get();
    return heapAllocator.reallocate(ptr, size);
}
