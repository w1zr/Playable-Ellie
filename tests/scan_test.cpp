// Offline signature test. Maps a game executable from disk the same way
// Windows lays it out in memory and runs the mod's own resolver
// (src/resolve.cpp) on it: everything Playable Ellie hooks or calls must be
// found, must match what w1z's menu 1.2.0 found (verified in game) for the
// known builds, and a damaged image must be rejected instead of yielding a
// bogus address.
//
// usage: scan_test.exe <path to tlou-i.exe> [<path to tlou-i-l.exe> ...]
#include "../src/dscan.h"
#include "../src/engine_defs.h"
#include "../src/resolve.h"
#include "../src/scan.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (cond) { std::printf("  PASS  "); }               \
        else { std::printf("  FAIL  "); ++g_failures; }      \
        std::printf(__VA_ARGS__);                            \
        std::printf("\n");                                   \
    } while (0)

static bool MapImage(const char* path, std::vector<uint8_t>& image, size_t& textEnd, uint32_t& stamp) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (file.size() < 0x400) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(file.data());
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
    stamp = nt->FileHeader.TimeDateStamp;
    image.assign(nt->OptionalHeader.SizeOfImage, 0);
    std::memcpy(image.data(), file.data(), nt->OptionalHeader.SizeOfHeaders);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    textEnd = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const size_t n = (std::min)(sec[i].SizeOfRawData, sec[i].Misc.VirtualSize);
        std::memcpy(image.data() + sec[i].VirtualAddress, file.data() + sec[i].PointerToRawData, n);
        if (!textEnd && (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
            textEnd = sec[i].VirtualAddress + sec[i].Misc.VirtualSize;
    }
    return true;
}

static void TestParser() {
    std::printf("parser\n");
    auto p = scan::Parse("32 C9 ?? 0d");
    CHECK(p.bytes.size() == 4 && p.bytes[0] == 0x32 && p.bytes[2] == -1 && p.bytes[3] == 0x0D, "parse hex and wildcards");
    const uint8_t buf[] = {0x00, 0x32, 0xC9, 0x55, 0x0D, 0x32, 0xC9, 0x00, 0x0D};
    auto hits = scan::FindAll(buf, sizeof(buf), p);
    CHECK(hits.size() == 2 && hits[0] == 1 && hits[1] == 5, "FindAll finds both wildcard matches");
    CHECK(scan::FindAll(buf, 3, p).empty(), "FindAll does not read past the buffer");
    CHECK(dscan::Fnv1a64("a") == 0xaf63dc4c8601ec8cull && dscan::Fnv1a64("") == 0xcbf29ce484222325ull,
          "FNV-1a 64 matches the reference values");
    static_assert(sizeof(engine::kEllieOnlyPackages) / sizeof(char*) == 49, "Ellie's package list: 49 packages");
}

// RVAs found in tlou-i.exe 1.1.4.0 (build 18995818) by w1z's menu 1.2.0 and
// written down in its research notes; that build of the menu was tested in game.
struct Known {
    uint32_t stamp;
    size_t restart, packageMgr, packageLoaded, rebuildWants, lookResolve, craftFlag;
};
static const Known kKnown[] = {
    {0x685BEBE1, 0x1CCB8B8, 0x6433130, 0x22DAA44, 0x23110E4, 0x1FBDECC, 0x36235EA},
};

static void TestImage(const char* path) {
    std::printf("image %s\n", path);
    std::vector<uint8_t> image;
    size_t textEnd = 0;
    uint32_t stamp = 0;
    if (!MapImage(path, image, textEnd, stamp)) {
        CHECK(false, "could not read file");
        return;
    }
    std::printf("        timestamp 0x%08X, code ends at 0x%zX, image 0x%zX\n", stamp, textEnd, image.size());
    const uint8_t* b = image.data();
    const resolve::Result r = resolve::Find(b, textEnd, image.size());
    const resolve::Addresses& a = r.a;
    for (const auto& e : r.errors) std::printf("        not found: %s\n", e.c_str());
    CHECK(r.errors.empty(), "every signature found (%zu not found)", r.errors.size());
    CHECK(r.RequiredOk(), "every required part found, so the mod would turn on");

    auto code = [&](size_t v) { return v > 0x1000 && v < textEnd; };
    auto data = [&](size_t v) { return v >= textEnd && v < image.size(); };
    CHECK(code(a.gameLoop), "GameLoopUpdate 0x%zX", a.gameLoop);
    CHECK(code(a.coreSpawn), "core player spawn 0x%zX (via Spawn Player, default character t1x-joel)", a.coreSpawn);
    CHECK(code(a.restart), "GameRestartCheckpoint 0x%zX", a.restart);
    CHECK(data(a.playerHandle) && code(a.playerLookup), "player handle 0x%zX, lookup 0x%zX", a.playerHandle, a.playerLookup);
    CHECK(data(a.levelSets.mgrGlobal) && code(a.levelSets.setLevelSet) && code(a.levelSets.rebuildWants),
          "level manager 0x%zX, SetLevelSet 0x%zX, want-load rebuild 0x%zX", a.levelSets.mgrGlobal, a.levelSets.setLevelSet,
          a.levelSets.rebuildWants);
    CHECK(data(a.packageMgr) && code(a.packageLoaded), "package manager 0x%zX, IsLoaded 0x%zX", a.packageMgr, a.packageLoaded);
    CHECK(code(a.lookResolve), "look resolver 0x%zX", a.lookResolve);
    CHECK(data(a.ellieCraftFlag), "Ellie crafting flag 0x%zX", a.ellieCraftFlag);
    CHECK(code(a.allRecipes), "All Recipes callback 0x%zX", a.allRecipes);
    for (const auto& k : kKnown)
        if (k.stamp == stamp)
            CHECK(a.restart == k.restart && a.packageMgr == k.packageMgr && a.packageLoaded == k.packageLoaded &&
                      a.levelSets.rebuildWants == k.rebuildWants && a.lookResolve == k.lookResolve &&
                      a.ellieCraftFlag == k.craftFlag,
                  "matches the addresses w1z's menu 1.2.0 used in game for this build");

    // The hooked functions must start with the prologue the hooks expect.
    CHECK(scan::FindAll(b + a.gameLoop, 40, scan::Parse(engine::kGameLoopPrologue)).size() == 1 &&
              scan::FindAll(b + a.coreSpawn, 25, scan::Parse(engine::kCorePrologue)).size() == 1 &&
              scan::FindAll(b + a.lookResolve, 96, scan::Parse(engine::kLookResolve)).size() == 1,
          "hook targets start with the expected code");

    // Damaged images: each must switch its part off instead of guessing.
    {
        auto copy = image;
        const auto hits = scan::FindAll(copy.data(), textEnd, scan::Parse(engine::kLookResolve));
        for (size_t h : hits) copy[h + 0x15] ^= 0xFF; // sub rsp, 70h -> another frame size
        const auto bad = resolve::Find(copy.data(), textEnd, copy.size());
        CHECK(!bad.a.lookResolve && bad.RequiredOk(), "a changed look resolver only switches the companion off");
    }
    {
        auto copy = image;
        const auto names = dscan::FindCString(copy.data(), copy.size(), engine::kGameLoopName);
        for (size_t n : names) copy[n] = 'X';
        const auto bad = resolve::Find(copy.data(), textEnd, copy.size());
        CHECK(!bad.a.gameLoop && !bad.RequiredOk(), "no game loop: the mod stays off");
    }
    {
        auto copy = image;
        const auto spawn = resolve::Find(copy.data(), textEnd, copy.size()).a.coreSpawn;
        if (spawn) copy[spawn] = 0xCC; // e.g. another mod hooked it
        const auto bad = resolve::Find(copy.data(), textEnd, copy.size());
        CHECK(!bad.a.coreSpawn && !bad.RequiredOk(), "a hooked or changed player spawn: the mod stays off");
    }
    {
        auto copy = image;
        const auto reads = scan::FindAll(copy.data(), textEnd, scan::Parse(engine::kEllieCraftFlagRead));
        for (size_t at : reads) copy[at + 4] = 0x0A; // jnz +9 -> +10
        const auto bad = resolve::Find(copy.data(), textEnd, copy.size());
        CHECK(!bad.a.ellieCraftFlag && bad.RequiredOk(), "a changed crafting check only switches crafting off");
    }
}

int main(int argc, char** argv) {
    TestParser();
    for (int i = 1; i < argc; ++i) TestImage(argv[i]);
    std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "ALL PASSED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
