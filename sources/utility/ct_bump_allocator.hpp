#pragma once
#include "ct_attributes.hpp"
#include <utility>
#include <cstddef>
#include <cstdint>

class ct_bump_allocator {
public:
    ct_bump_allocator() noexcept = default;
    ct_bump_allocator(std::uint8_t *data, std::size_t cap, std::size_t al = alignof(std::max_align_t)) noexcept;
    ct_bump_allocator(ct_bump_allocator &&o) noexcept;
    ct_bump_allocator &operator=(ct_bump_allocator &&o) noexcept;

public:
    std::size_t allocated() const { return top_; }
    std::size_t capacity() const { return cap_; }
    std::size_t pad_size(std::size_t size) const noexcept;
    void clear() noexcept { top_ = 0; }
    bool empty() const noexcept { return top_ == 0; }
    CT_ATTRIBUTE_MALLOC void *alloc(std::size_t size) noexcept;
    CT_ATTRIBUTE_MALLOC void *unchecked_alloc(std::size_t size) noexcept;
    void free(const void *ptr) noexcept;

    template <class T> CT_ATTRIBUTE_MALLOC T *typed_alloc(std::size_t count = 1)
    {
        return (T *)alloc(count * sizeof(T));
    }
    template <class T> CT_ATTRIBUTE_MALLOC T *unchecked_typed_alloc(std::size_t count = 1)
    {
        return (T *)unchecked_alloc(count * sizeof(T));
    }

private:
    std::uint8_t *data_ = nullptr;
    std::size_t top_ = 0;
    std::size_t cap_ = 0;
    std::size_t mask_ = 0;
    struct descriptor;
};
