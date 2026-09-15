#include "text.hpp"

std::string to_ascii_punctuation(const std::string& s) {
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);

        // U+2000..U+206F general punctuation: E2 80 xx
        if (c == 0xE2 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x80) {
            const auto b2 = static_cast<unsigned char>(s[i + 2]);
            switch (b2) {
                case 0x98: case 0x99: case 0x9A: case 0x9B: // ' ' ‚ ›
                case 0xB2:                                  // prime
                    out += '\'';
                    i += 3;
                    continue;
                case 0x9C: case 0x9D: case 0x9E: case 0x9F: // " " „ ‟
                case 0xB3:                                  // double prime
                    out += '"';
                    i += 3;
                    continue;
                case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: // hyphen, dashes
                    out += '-';
                    i += 3;
                    continue;
                case 0xA6: // ellipsis
                    out += "...";
                    i += 3;
                    continue;
                case 0xA2: // bullet
                    out += '*';
                    i += 3;
                    continue;
                default:
                    break;
            }
        } else if (c == 0xC2 && i + 1 < s.size()) {
            const auto b1 = static_cast<unsigned char>(s[i + 1]);
            if (b1 == 0xA0) { out += ' '; i += 2; continue; } // non-breaking space
            if (b1 == 0xAD) { i += 2; continue; }             // soft hyphen
        } else if (c == 0xEF && i + 2 < s.size() &&
                   static_cast<unsigned char>(s[i + 1]) == 0xBB &&
                   static_cast<unsigned char>(s[i + 2]) == 0xBF) {
            i += 3; // byte order mark
            continue;
        }

        out += s[i];
        ++i;
    }
    return out;
}
