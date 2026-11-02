#include "resolve.h"

#include "engine_defs.h"
#include "scan.h"

#include <cstring>

namespace resolve {

namespace {

size_t Rel(const uint8_t* image, size_t at) { // rel32 operand at `at`, from the end of the operand
    int32_t r;
    std::memcpy(&r, image + at, 4);
    return static_cast<size_t>(static_cast<int64_t>(at) + 4 + r);
}

// The pattern matches exactly at `at`.
bool MatchesAt(const uint8_t* image, size_t at, const char* pattern) {
    const scan::Pattern p = scan::Parse(pattern);
    const auto hits = scan::FindAll(image + at, p.bytes.size(), p);
    return hits.size() == 1;
}

// Spawn Player callback -> player-spawn function -> core spawn, each checked.
size_t FindCoreSpawn(const uint8_t* b, size_t textEnd, size_t cb, std::string& err) {
    if (cb + 0x60 > textEnd) return err = "Spawn Player's callback is outside the code", 0;
    const auto call = scan::FindAll(b + cb, 0x60, scan::Parse(engine::kSpawnCall));
    if (call.size() != 1) return err = "Spawn Player's code was not recognised", 0;
    const size_t spawn = dscan::FirstCall(b, cb + call[0] + 15, 1);
    if (!spawn || spawn + 0x200 > textEnd) return err = "the player-spawn function is outside the code", 0;
    const auto def = scan::FindAll(b + spawn, 0x100, scan::Parse(engine::kDefaultCharacter));
    uint64_t defaultId = 0;
    if (!def.empty()) std::memcpy(&defaultId, b + spawn + def[0] + 2, 8);
    if (defaultId != dscan::Fnv1a64(engine::kJoel))
        return err = "the player-spawn function was not recognised (default character changed)", 0;
    const auto coreCall = scan::FindAll(b + spawn, 0x200, scan::Parse(engine::kCoreCall));
    if (coreCall.size() != 1) return err = "the core player spawn was not found", 0;
    const size_t core = dscan::FirstCall(b, spawn + coreCall[0] + 5, 1);
    if (!core || core + 32 > textEnd || !MatchesAt(b, core, engine::kCorePrologue))
        return err = "the core player spawn has an unexpected layout", 0;
    return core;
}

// The package manager and IsLoaded, from the call sites that check a package
// before loading it. Every matching site must agree.
bool FindPackageChecks(const uint8_t* b, size_t textEnd, Addresses& a, std::string& err) {
    const auto sites = scan::FindAll(b, textEnd, scan::Parse(engine::kPackageCheckCall));
    size_t mgr = 0, loaded = 0;
    int good = 0;
    for (size_t s : sites) {
        const size_t fn = Rel(b, s + 25), check = Rel(b, s + 8);
        if (check + 0x60 > textEnd || fn + 0x40 > textEnd) continue;
        if (!MatchesAt(b, fn, engine::kLoadPackagePrologue) || !MatchesAt(b, check, engine::kPackageLoadingCheck) ||
            !MatchesAt(b, check + engine::kPackageLoadedOffset, engine::kPackageLoadedCheck))
            continue;
        const size_t m = Rel(b, s + 3), l = check + engine::kPackageLoadedOffset;
        if (good++ == 0) mgr = m, loaded = l;
        else if (m != mgr || l != loaded) return err = "package check call sites disagree", false;
    }
    if (!good) return err = "not found (" + std::to_string(sites.size()) + " candidate sites)", false;
    a.packageMgr = mgr;
    a.packageLoaded = loaded;
    return true;
}

} // namespace

bool Result::RequiredOk() const {
    return a.gameLoop && a.coreSpawn && a.restart && a.playerHandle && a.playerLookup && a.levelSets.mgrGlobal &&
           a.levelSets.rebuildWants && a.packageMgr && a.packageLoaded;
}

Result Find(const uint8_t* b, size_t textEnd, size_t imageSize) {
    Result r;
    Addresses& a = r.a;
    std::string err;
    auto fail = [&](const char* what) {
        r.errors.push_back(std::string(what) + ": " + (err.empty() ? "not found" : err));
        err.clear();
    };

    a.gameLoop = dscan::FindFunctionByName(b, textEnd, imageSize, engine::kGameLoopName, engine::kGameLoopPrologue, err);
    if (!a.gameLoop) fail("game loop");
    a.restart = dscan::FindFunctionByName(b, textEnd, imageSize, engine::kRestartName, engine::kRestartPrologue, err);
    if (!a.restart) fail("restart checkpoint");

    // Developer-menu items. Constructors come from items whose kind is known.
    dscan::Index idx(b, textEnd, imageSize);
    for (const char* l : {engine::kSpawnPlayerLabel, engine::kBoolCallbackAnchor, engine::kFunctionAnchor, engine::kAllRecipesLabel})
        idx.WantLabel(l);
    idx.Build();
    const auto& boolCb = idx.Items(engine::kBoolCallbackAnchor);
    const auto& func = idx.Items(engine::kFunctionAnchor);
    const size_t itemBoolCb = boolCb.size() == 1 ? boolCb[0].ctor : 0, itemFunction = func.size() == 1 ? func[0].ctor : 0;
    if (!itemBoolCb || !itemFunction || itemBoolCb == itemFunction) {
        err = "item constructors not recognised";
        fail("developer menu");
    } else {
        if (const size_t cb = idx.ItemValue(engine::kSpawnPlayerLabel, itemBoolCb, false, err)) {
            a.coreSpawn = FindCoreSpawn(b, textEnd, cb, err);
            if (!a.coreSpawn) fail("player spawn");
        } else {
            fail("player spawn");
        }
        // Player lookup: sub rsp,28h; lea rcx,[rip+handle]; mov r9d,edx; call lookup
        const size_t gec = idx.ItemValue(engine::kFunctionAnchor, itemFunction, false, err);
        if (gec && gec + 0x20 < textEnd && MatchesAt(b, gec, engine::kPlayerLookupStart)) {
            a.playerHandle = Rel(b, gec + 7);
            a.playerLookup = dscan::FirstCall(b, gec + 14, 1);
            if (a.playerHandle >= imageSize || a.playerLookup >= textEnd) a.playerHandle = a.playerLookup = 0;
        }
        if (!a.playerLookup) fail("player lookup");
        a.allRecipes = idx.ItemValue(engine::kAllRecipesLabel, itemBoolCb, false, err);
        if (a.allRecipes >= textEnd) a.allRecipes = 0;
        if (!a.allRecipes) fail("All Recipes");
    }

    if (!dscan::FindLevelSetLoader(b, textEnd, imageSize, a.levelSets, err)) {
        a.levelSets = {};
        fail("level-set loader");
    }
    if (!FindPackageChecks(b, textEnd, a, err)) fail("package checks");

    const auto look = scan::FindAll(b, textEnd, scan::Parse(engine::kLookResolve));
    if (look.size() == 1) a.lookResolve = look[0];
    else r.errors.push_back("look resolver: " + std::to_string(look.size()) + " matches (expected 1)");
    if (a.lookResolve) {
        const auto skin = scan::FindAll(b, textEnd, scan::Parse(engine::kSkinLookCall));
        const size_t call = skin.size() == 1 ? skin[0] + engine::kSkinLookCallAt : 0;
        int32_t rel = 0;
        if (call) std::memcpy(&rel, b + call + 1, 4);
        if (call && call + 5 + rel == a.lookResolve) a.skinLookReturn = skin[0] + engine::kSkinLookReturn;
        else r.errors.push_back("Skins menu look call: " + std::to_string(skin.size()) + " matches (expected 1 calling the look resolver)");
        const auto level = scan::FindAll(b, textEnd, scan::Parse(engine::kLevelLookCall));
        const size_t lcall = level.size() == 1 ? level[0] + engine::kLevelLookCallAt : 0;
        rel = 0;
        if (lcall) std::memcpy(&rel, b + lcall + 1, 4);
        if (lcall && lcall + 5 + rel == a.lookResolve) a.levelLookReturn = level[0] + engine::kLevelLookReturn;
        else r.errors.push_back("level-load look call: " + std::to_string(level.size()) + " matches (expected 1 calling the look resolver)");
    }
    a.ellieCraftFlag = dscan::FindEllieCraftFlag(b, textEnd, imageSize, err);
    if (!a.ellieCraftFlag) fail("Ellie crafting flag");
    return r;
}

} // namespace resolve
