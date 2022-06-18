/* The MIT License

   Copyright (c) 2022      Jean Pierre Cimalando <jp-dev@gmx.com>
   Copyright (c) 2008-2011 Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

//NOTE: This port is a derivative work from the Klib library
//      found originally at https://github.com/attractivechaos/klib
//SPDX-License-Identifier: MIT

#include <iterator>
#include <functional>
#include <cstddef>

#ifndef HEAPSORT_NAMESPACE_BEGIN
#   define HEAPSORT_NAMESPACE_BEGIN namespace ks {
#endif
#ifndef HEAPSORT_NAMESPACE_END
#   define HEAPSORT_NAMESPACE_END }
#endif

HEAPSORT_NAMESPACE_BEGIN

namespace heapsort_detail {

template <class T>
T *address_from_iterator(T *x)
{
    return x;
}

template <class T>
typename T::pointer address_from_iterator(const T &x)
{
    return x.operator->();
}

template <class type_t, class compare_t = std::less<type_t>>
void heapadjust(std::size_t i, std::size_t n, type_t l[], const compare_t &lt)
{
    std::size_t k = i;
    type_t tmp = std::move(l[i]);
    while ((k = (k << 1) + 1) < n) {
        if (k != n - 1 && lt(l[k], l[k+1])) ++k;
        if (lt(l[k], tmp)) break;
        l[i] = std::move(l[k]); i = k;
    }
    l[i] = std::move(tmp);
}

template <class iterator_t, class compare_t = std::less<typename std::iterator_traits<iterator_t>::value_type>>
void heapmake(iterator_t first, iterator_t last, const compare_t &lt)
{
    typename std::iterator_traits<iterator_t>::pointer l = address_from_iterator(first);
    std::size_t lsize = std::distance(first, last);
    std::size_t i;
    for (i = lsize >> 1; i-- > 0; )
        heapadjust(i, lsize, l, lt);
}

template <class iterator_t, class compare_t = std::less<typename std::iterator_traits<iterator_t>::value_type>>
void heapsort(iterator_t first, iterator_t last, const compare_t &lt)
{
    typename std::iterator_traits<iterator_t>::pointer l = address_from_iterator(first);
    std::size_t lsize = std::distance(first, last);
    std::size_t i;
    for (i = lsize; i-- > 0; ) {
        std::swap(*l, l[i]);
        heapadjust(0, i, l, lt);
    }
}

} // namespace heapsort_detail

template <class iterator_t, class compare_t = std::less<typename std::iterator_traits<iterator_t>::value_type>>
void heapsort(iterator_t first, iterator_t last, const compare_t &lt = {})
{
    heapsort_detail::heapmake(first, last, lt);
    heapsort_detail::heapsort(first, last, lt);
}

HEAPSORT_NAMESPACE_END
