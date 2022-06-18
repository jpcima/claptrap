#include "url_helpers.hpp"

std::string URL_decode(std::string_view text)
{
    std::string result;
    result.reserve(text.size());

    auto hexdigit = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };

    for (size_t i = 0, n = text.size(); i < n; ) {
        char lead = text[i++];
        int percent = -1;
        if (lead == '%' && n - i >= 2) {
            int d1 = hexdigit(text[i]);
            int d2 = hexdigit(text[i + 1]);
            if (d1 != -1 && d2 != -1) {
                percent = (d1 << 4) | d2;
                i += 2;
            }
        }
        if (percent == -1)
            result.push_back(lead);
        else
            result.push_back((char)(unsigned char)percent);
    }

    return result;
}
