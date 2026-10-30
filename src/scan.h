// Byte-pattern scanning. Kept free of Windows/game dependencies so the same
// code is exercised by the offline test (tests/).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace scan {

struct Pattern {
    std::vector<int> bytes; // -1 = wildcard
};

// Parses "32 C9 38 0D ?? ?? ..." into a Pattern.
Pattern Parse(const char* text);

// Returns every offset in [data, data+size) where the pattern matches.
std::vector<size_t> FindAll(const uint8_t* data, size_t size, const Pattern& p);

} // namespace scan
