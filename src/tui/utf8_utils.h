#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace szip::tui {

inline std::string pathToUtf8(const std::filesystem::path& p) {
#if defined(__cpp_char8_t)
    auto s = p.u8string();
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
#else
    return p.u8string();
#endif
}

namespace detail {

inline uint32_t decodeUtf8(const std::string& s, size_t& pos) {
    auto c = static_cast<unsigned char>(s[pos]);
    uint32_t cp;
    int len;

    if (c < 0x80)      { cp = c;        len = 1; }
    else if (c < 0xC0) { ++pos; return 0xFFFD; }
    else if (c < 0xE0) { cp = c & 0x1F; len = 2; }
    else if (c < 0xF0) { cp = c & 0x0F; len = 3; }
    else if (c < 0xF8) { cp = c & 0x07; len = 4; }
    else                { ++pos; return 0xFFFD; }

    for (int i = 1; i < len; ++i) {
        if (pos + i >= s.size()) { pos += len; return 0xFFFD; }
        cp = (cp << 6) | (static_cast<unsigned char>(s[pos + i]) & 0x3F);
    }
    pos += len;
    return cp;
}

inline int codepointWidth(uint32_t cp) {
    if (cp < 0x20) return 0;
    // CJK, Hangul, fullwidth, emoji and other double-width ranges
    if ((cp >= 0x1100 && cp <= 0x115F) ||
        cp == 0x2329 || cp == 0x232A ||
        (cp >= 0x2E80 && cp <= 0x303E) ||
        (cp >= 0x3040 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7AF) ||
        (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE10 && cp <= 0xFE6F) ||
        (cp >= 0xFF01 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        (cp >= 0x1F000 && cp <= 0x1FAFF) ||
        (cp >= 0x20000 && cp <= 0x2FA1F))
        return 2;
    return 1;
}

}  // namespace detail

inline int stringDisplayWidth(const std::string& s) {
    int w = 0;
    size_t pos = 0;
    while (pos < s.size())
        w += detail::codepointWidth(detail::decodeUtf8(s, pos));
    return w;
}

inline std::string truncateToWidth(const std::string& s, int max_width) {
    if (max_width <= 0) return {};

    int total = stringDisplayWidth(s);
    if (total <= max_width) return s;

    constexpr int kEllipsisWidth = 2;
    int target = max_width - kEllipsisWidth;
    if (target <= 0) return "..";

    int w = 0;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t prev = pos;
        auto cp = detail::decodeUtf8(s, pos);
        int cw = detail::codepointWidth(cp);
        if (w + cw > target) { pos = prev; break; }
        w += cw;
    }
    return s.substr(0, pos) + "..";
}

}  // namespace szip::tui
