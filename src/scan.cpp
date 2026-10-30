#include "scan.h"

#include <cstdlib>

namespace scan {

Pattern Parse(const char* text) {
    Pattern p;
    const char* s = text;
    while (*s) {
        while (*s == ' ') ++s;
        if (!*s) break;
        if (s[0] == '?') {
            p.bytes.push_back(-1);
            while (*s == '?') ++s;
        } else {
            char* end = nullptr;
            p.bytes.push_back(static_cast<int>(std::strtoul(s, &end, 16)));
            s = end;
        }
    }
    return p;
}

std::vector<size_t> FindAll(const uint8_t* data, size_t size, const Pattern& p) {
    std::vector<size_t> hits;
    const size_t n = p.bytes.size();
    if (n == 0 || size < n) return hits;
    const int first = p.bytes[0];
    for (size_t i = 0; i + n <= size; ++i) {
        if (first >= 0 && data[i] != first) continue;
        size_t k = 1;
        for (; k < n; ++k) {
            const int b = p.bytes[k];
            if (b >= 0 && data[i + k] != b) break;
        }
        if (k == n) hits.push_back(i);
    }
    return hits;
}

} // namespace scan
