#include "ct_bump_allocator.hpp"
#include "ct_assert.hpp"

struct ct_bump_allocator::descriptor {
    std::size_t size;
#if defined(CT_ASSERTIONS) || !defined(NDEBUG)
    mutable std::uint32_t magic;
#endif
};

#if defined(CT_ASSERTIONS) || !defined(NDEBUG)
static constexpr std::uint32_t block_magic = 0x12345678u;
#endif

ct_bump_allocator::ct_bump_allocator(ct_bump_allocator &&o) noexcept
    : data_(o.data_), top_(o.top_), cap_(o.cap_)
{
    o.top_ = 0;
    o.cap_ = 0;
}

ct_bump_allocator::ct_bump_allocator(std::uint8_t *data, std::size_t cap, std::size_t al) noexcept
    : data_(data), cap_(cap), mask_(al - 1)
{
}

ct_bump_allocator &ct_bump_allocator::operator=(ct_bump_allocator &&o) noexcept
{
    data_ = o.data_;
    top_ = o.top_;
    cap_  = o.cap_;
    o.top_ = 0;
    o.cap_ = 0;
    return *this;
}

std::size_t ct_bump_allocator::pad_size(std::size_t size) const noexcept
{
    return (size + mask_) & ~mask_;
}

void *ct_bump_allocator::alloc(std::size_t size) noexcept
{
    std::size_t totalsize = pad_size(size + sizeof(descriptor));
    if (totalsize > cap_ - top_)
        return nullptr;
    std::uint8_t *block = &data_[top_];
    top_ += totalsize;
    descriptor *desc = (descriptor *)(block + totalsize - sizeof(descriptor));
    desc->size = size;
#if defined(CT_ASSERTIONS) || !defined(NDEBUG)
    desc->magic = block_magic;
#endif
    return block;
}

void *ct_bump_allocator::unchecked_alloc(std::size_t size) noexcept
{
    std::size_t totalsize = pad_size(size + sizeof(descriptor));
    CT_ASSERT(totalsize <= cap_ - top_);
    std::uint8_t *block = &data_[top_];
    top_ += totalsize;
    descriptor *desc = (descriptor *)(block + totalsize - sizeof(descriptor));
    desc->size = size;
#if defined(CT_ASSERTIONS) || !defined(NDEBUG)
    desc->magic = block_magic;
#endif
    return block;
}

void ct_bump_allocator::free(const void *ptr) noexcept
{
    CT_ASSERT(ptr);
    (void)ptr;
    const std::uint8_t *ptop = &data_[top_];
    const descriptor *desc = (const descriptor *)(ptop - sizeof(descriptor));
#if defined(CT_ASSERTIONS) || !defined(NDEBUG)
    CT_ASSERT(desc->magic == block_magic);
    desc->magic = 0;
#endif
    std::size_t totalsize = pad_size(desc->size + sizeof(descriptor));
#if defined(CT_ASSERTIONS) || !defined(NDEBUG)
    const void *plast = ptop - totalsize;
    CT_ASSERT(ptr == plast);
#endif
    top_ -= totalsize;
}
