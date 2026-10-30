// Finds every game function and global Playable Ellie uses, by signature, in
// an image laid out as in memory (the loaded game, or the offline test's own
// mapping of the exe). No Windows or game dependencies, so tests/scan_test.cpp
// runs exactly this code on tlou-i.exe and tlou-i-l.exe.
#pragma once
#include "dscan.h"

#include <cstddef>
#include <string>
#include <vector>

namespace resolve {

// RVAs; 0 = not found (the reason is in `errors`).
struct Addresses {
    // Required: without any of these the mod stays off.
    size_t gameLoop = 0;     // GameLoopUpdate (hooked: the mod's game-thread work runs there)
    size_t coreSpawn = 0;    // core player spawn (hooked: swaps Joel's character id for Ellie's)
    size_t restart = 0;      // GameRestartCheckpoint(bool, bool)
    size_t playerHandle = 0; // the developer menu's player handle and lookup (is a player in the game?)
    size_t playerLookup = 0;
    dscan::LevelSetLoader levelSets; // LevelMgr global + want-load rebuild (streams Ellie's animations in)
    size_t packageMgr = 0;   // package manager global and IsLoaded(mgr, hash): waits for her animations
    size_t packageLoaded = 0;
    // Optional: each one only switches its own feature off.
    size_t lookResolve = 0;    // look resolver (hooked: the companion Ellie becomes Joel)
    size_t ellieCraftFlag = 0; // lets an Ellie player craft outside Winter
    size_t allRecipes = 0;     // developer menu's All Recipes callback
};

struct Result {
    Addresses a;
    std::vector<std::string> errors; // one line per thing not found
    bool RequiredOk() const;
};

Result Find(const uint8_t* image, size_t textEnd, size_t imageSize);

} // namespace resolve
