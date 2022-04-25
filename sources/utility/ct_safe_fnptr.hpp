#pragma once
#include "ct_assert.hpp"
#include <utility>

namespace ct {
namespace detail {

template <class FnPtr>
struct safe_fnptr_invoker;

// definition for void functions
template <class... FnArgs>
struct safe_fnptr_invoker<void (*)(FnArgs...)> {
    explicit inline safe_fnptr_invoker(void (*fnptr)(FnArgs...)) : m_fnptr{fnptr} {}
    template <class... Args> inline void invoke(Args &&... args)
    {
        if (m_fnptr) m_fnptr(std::forward<Args>(args)...);
    }
    void (*m_fnptr)(FnArgs...) = nullptr;
};

// definition for non-void functions
template <class FnRet, class... FnArgs>
struct safe_fnptr_invoker<FnRet (*)(FnArgs...)> {
    explicit inline safe_fnptr_invoker(FnRet (*fnptr)(FnArgs...)) : m_fnptr{fnptr} {}
    template <class... Args> inline FnRet invoke(Args &&... args)
    {
        return m_fnptr ? m_fnptr(std::forward<Args>(args)...) : FnRet{};
    }
    FnRet (*m_fnptr)(FnArgs...) = nullptr;
};

// result type of a function pointer
template <class T>
struct fnptr_result;

template <class Ret, class... Args>
struct fnptr_result<Ret (*)(Args...)> { using type = Ret; };

template <class T>
using fnptr_result_t = typename fnptr_result<T>::type;

} // namespace detail
} // namespace ct

template <class Fn, class... Args>
ct::detail::fnptr_result_t<Fn *> ct_safe_fnptr_call(Fn *fn, Args &&... args)
{
    return ct::detail::safe_fnptr_invoker<Fn *>{fn}.invoke(std::forward<Args>(args)...);
}

template <class Self, class AccessFn, class... Args>
inline auto ct_safe_fnptr_access_call(Self *self, AccessFn *access, Args &&... args)
{
    CT_ASSERT(self);
    return ct_safe_fnptr_call(access(self), std::forward<Args>(args)...);
}
