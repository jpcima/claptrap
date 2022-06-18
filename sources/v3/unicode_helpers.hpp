#pragma once
#include <utf/utf.hpp>
#include <nonstd/span.hpp>
#include <string_view>
#include <string>

template <class Y, class X>
std::basic_string<Y> UTF_convert(std::basic_string_view<X> strX)
{
    std::basic_string<Y> strY;
    std::size_t sizeY = 0;
    typename std::basic_string_view<X>::const_iterator itX;
    typename std::basic_string<Y>::iterator itY;

    //
    itX = strX.cbegin();
    while (itX != strX.cend()) {
        utf::code_point cp = utf::utf_traits<X>::decode(itX, strX.end());
        if (cp == utf::incomplete) break;
        if (cp == utf::illegal) continue;
        sizeY += static_cast<std::size_t>(utf::utf_traits<Y>::width(cp));
    }
    strY.resize(sizeY);

    //
    itX = strX.cbegin();
    itY = strY.begin();
    while (itX != strX.cend()) {
        utf::code_point cp = utf::utf_traits<X>::decode(itX, strX.end());
        if (cp == utf::incomplete) break;
        if (cp == utf::illegal) continue;
        itY = utf::utf_traits<Y>::encode(cp, itY);
    }

    return strY;
}

template <class Y, class X>
std::basic_string<Y> UTF_convert(const std::basic_string<X> &strX)
{
    return UTF_convert<Y, X>(std::basic_string_view<X>{strX});
}

template <class Y, class X>
std::basic_string<Y> UTF_convert(const X *strX)
{
    return UTF_convert<Y, X>(std::basic_string_view<X>{strX});
}

//------------------------------------------------------------------------------
template <class Y, class X>
void UTF_copy(nonstd::span<Y> strY, std::basic_string_view<X> strX)
{
    if (strY.size() < 1)
        return;

    typename std::basic_string_view<X>::const_iterator itX;
    Y *itY;
    Y *endY;

    itX = strX.cbegin();
    itY = strY.data();
    endY = strY.data() + strY.size() - 1;
    while (itX != strX.cend()) {
        utf::code_point cp = utf::utf_traits<X>::decode(itX, strX.end());
        if (cp == utf::incomplete) break;
        if (cp == utf::illegal) continue;
        std::size_t cplen = static_cast<std::size_t>(utf::utf_traits<Y>::width(cp));
        if (itY + cplen > endY) break;
        itY = utf::utf_traits<Y>::encode(cp, itY);
    }
    *itY = Y{0};
}

template <class Y, class X>
void UTF_copy(nonstd::span<Y> strY, const std::basic_string<X> &strX)
{
    return UTF_copy<Y, X>(strY, std::basic_string_view<X>{strX});
}

template <class Y, class X>
void UTF_copy(nonstd::span<Y> strY, const X *strX)
{
    return UTF_copy<Y, X>(strY, std::basic_string_view<X>{strX});
}

template <class Y, class X, std::size_t SizeY>
void UTF_copy(Y (&startY)[SizeY], std::basic_string_view<X> strX)
{
    UTF_copy<Y, X>(nonstd::span<Y>{startY, startY + SizeY}, strX);
}

template <class Y, class X, std::size_t SizeY>
void UTF_copy(Y (&startY)[SizeY], const std::basic_string<X> &strX)
{
    UTF_copy<Y, X>(nonstd::span<Y>{startY, startY + SizeY}, std::basic_string_view<X>{strX});
}

template <class Y, class X, std::size_t SizeY>
void UTF_copy(Y (&startY)[SizeY], const X *strX)
{
    UTF_copy<Y, X>(nonstd::span<Y>{startY, startY + SizeY}, std::basic_string_view<X>{strX});
}
