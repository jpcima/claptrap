#pragma once
#include <utility>

namespace ct {

template <class F>
class scope_guard {
public:
    explicit scope_guard(F &&f) : f(std::forward<F>(f)), a(true) {}
    scope_guard(scope_guard &&o) : f(std::move(o.f)), a(o.a) { o.a = false; }
    ~scope_guard() { if (a) f(); }
    void disarm() { a = false; }
private:
    F f;
    bool a;
    scope_guard(const scope_guard &) = delete;
    scope_guard &operator=(const scope_guard &) = delete;
};

template <class F> scope_guard<F> defer(F &&f)
{
    return scope_guard<F>(std::forward<F>(f));
}

} // namespace ct
