#pragma once
#if defined(_WIN32)
#   include <malloc.h>
#else
#   include <alloca.h>
#endif
#include <memory>
#include <cstdlib>
#include <cstddef>

//------------------------------------------------------------------------------
#if defined(_WIN32)
#   define CT_ALLOCA _alloca
#else
#   define CT_ALLOCA alloca
#endif

#define CT_ALLOCA_SPAN(count, /*type*/...) (nonstd::span<__VA_ARGS__>{  \
            (__VA_ARGS__ *)CT_ALLOCA(count * sizeof(__VA_ARGS__)), (count)})

//------------------------------------------------------------------------------
namespace ct {

class stdc_delete {
public:
    void operator()(void *x) const noexcept { std::free(x); }
};

template <class T>
using stdc_ptr = std::unique_ptr<T, stdc_delete>;

template <class T>
stdc_ptr<T> stdc_allocate()
{
    T *p = (T *)std::calloc(1, sizeof(T));
    if (!p)
        throw std::bad_alloc{};
    return stdc_ptr<T>{p};
}

template <class T>
stdc_ptr<T[]> stdc_allocate(std::size_t count)
{
    T *p = (T *)std::calloc(count, sizeof(T));
    if (!p)
        throw std::bad_alloc{};
    return stdc_ptr<T[]>{p};
}

} // namespace ct
