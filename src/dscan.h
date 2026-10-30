// Locating game code from strings in the game image.
//
// The retail exe still builds Naughty Dog's developer menu at startup. Each
// item is created as
//     lea r8/r9, [value or callback]   ; within 48 bytes before
//     lea rdx,  [label string]
//     call      Item constructor        ; within 32 bytes after
// so a developer-menu function is found by its exact label text. Other code
// is found from the profiling-name or command strings it references. Like
// scan.*, this file has no Windows or game dependencies so the offline test
// exercises exactly the same code. (From w1z's menu 1.2.0.)
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace dscan {

// 64-bit FNV-1a, the engine's StringId64 hash.
uint64_t Fnv1a64(const char* s);

// Offsets of a NUL-terminated string that is also preceded by a NUL.
std::vector<size_t> FindCString(const uint8_t* image, size_t size, const char* text);

struct Item {
    size_t site = 0;       // the `lea rdx, [label]`
    size_t ctor = 0;       // constructor called right after
    size_t r8 = 0, r9 = 0; // targets of `lea r8/r9, [rip+x]` before the label (0 = none)
};

// Builds, in one pass over [0, textEnd), the developer-menu items for each
// wanted label. Labels are matched exactly.
class Index {
public:
    Index(const uint8_t* image, size_t textEnd, size_t imageSize);

    void WantLabel(const std::string& label);
    void Build();

    const std::vector<Item>& Items(const std::string& label) const;

    // Every item for `label` built by `ctor` must agree on one non-zero r8
    // (or r9). Returns 0 (and sets error) on no match or disagreement.
    size_t ItemValue(const std::string& label, size_t ctor, bool r9, std::string& error) const;

private:
    size_t RipTarget(size_t dispOffset, size_t end) const;

    const uint8_t* image_;
    size_t textEnd_, imageSize_;
    std::set<std::string> wantedLabels_;
    std::map<size_t, std::string> labelAt_; // string rva -> label
    std::map<std::string, std::vector<Item>> items_;
};

// The function that contains a reference (lea) to the NUL-terminated `name`
// string and starts with `prologue` (a pattern as in scan::Parse), found by
// walking back from the reference. Exactly one is required; 0 otherwise.
size_t FindFunctionByName(const uint8_t* image, size_t textEnd, size_t imageSize, const char* name,
                          const char* prologue, std::string& error);

// LevelMgr's level-set code, found from the console's LoadLevelSet command
// (patterns in engine_defs.h): the LevelMgr pointer global and the function
// that rebuilds the want-load list from the current level set. Every check
// must pass; false (and error) otherwise.
struct LevelSetLoader {
    size_t mgrGlobal = 0, setLevelSet = 0, rebuildWants = 0;
};
bool FindLevelSetLoader(const uint8_t* image, size_t textEnd, size_t imageSize, LevelSetLoader& out, std::string& error);

// The game flag that lets an Ellie player craft outside the Winter world
// (patterns in engine_defs.h). Returns its RVA, or 0 (and error).
size_t FindEllieCraftFlag(const uint8_t* image, size_t textEnd, size_t imageSize, std::string& error);

// Target of the first `call rel32` at or after `from` within `span` bytes.
size_t FirstCall(const uint8_t* image, size_t from, size_t span);

} // namespace dscan
