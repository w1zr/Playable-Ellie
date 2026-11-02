#include "ellie.h"

#include "dscan.h"
#include "engine_defs.h"
#include "gameloop.h"
#include "log.h"
#include "overlay.h"
#include "settings.h"
#include "version.h"

#include "../third_party/minhook/include/MinHook.h"

#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ellie {

namespace {

using CoreSpawn = void*(__fastcall*)(void*, void*, void*, void*, uint64_t, void*);
using Restart = bool(__fastcall*)(bool, bool);
using PackageState = bool(__fastcall*)(void*, uint64_t);
using RebuildWants = void(__fastcall*)(void*);
using ResolveLook = void*(__fastcall*)(void*, uint64_t, void*, void*);

const uint8_t* g_base = nullptr;
CoreSpawn oCore = nullptr;
Restart g_restart = nullptr;
PackageState g_loaded = nullptr; // IsLoaded(package manager, name hash)
void** g_packageMgr = nullptr;
void** g_levelMgr = nullptr;      // LevelMgr and its want-load rebuild (see dscan::FindLevelSetLoader)
RebuildWants g_rebuildWants = nullptr;
const uint8_t* g_allRecipes = nullptr; // developer menu's All Recipes callback, or null
ResolveLook oResolveLook = nullptr;

// Settings (PlayableEllie.ini).
std::atomic<bool> g_on{false}; // Play as Ellie
bool g_joelBuddy = false;      // Joel as the companion (and its hook is in)
int g_outfit = 0;              // 0 = match the chapter, else index into g_ellieList
bool g_giveRecipes = true;
bool g_sounds = false;  // beeps (off by default; the on-screen messages replace them)
int g_toggleKey = 0;

// Character id -> name, for every Joel / Ellie player character the mod knows.
std::unordered_map<uint64_t, std::string> g_names;
std::unordered_map<uint64_t, uint64_t> g_toEllie, g_toJoel;
std::vector<std::string> g_ellieList; // fixed outfit choices (Outfit=1..n)

// The player the last spawn was swapped to: 0 = none, 1 = Ellie.
std::atomic<int> g_swappedTo{0};
// The last spawn was Ellie without a swap (one of Ellie's own chapters).
std::atomic<bool> g_nativeEllie{false};
// Counts player spawns, so Tick can check each swapped spawn once.
std::atomic<int> g_spawns{0};
// The thread inside the core player spawn (0 = none), and look log lines since it.
std::atomic<DWORD> g_playerSpawnThread{0};
std::atomic<DWORD> g_spawnThread{0}; // the thread that runs player spawns (the companion spawns there too)
// The look the mod gave the player at the last spawn (0 = not swapped to Ellie).
// The game can resolve the player's look again on a worker thread after the
// spawn, more often on CPUs with many cores (a player on Nexus, 2026-11-01:
// invisible body with only her head and backpack, gone with fewer CPU cores in
// the game's affinity). That look is never the companion's to change.
std::atomic<uint64_t> g_playerLook{0};
const uint8_t* g_skinLookReturn = nullptr; // the Skins menu's look call: never swapped
const uint8_t* g_levelLookReturn = nullptr; // the level-load look registration: never swapped
uint64_t g_ellieBase = 0; // t1x-ellie, Ellie without an outfit number (the Skins menu's preview)

// Outfit=0 dresses Ellie in the outfit the chapter's own Ellie (the companion)
// wears: the level always loads that one. Matching Joel's outfit number instead
// (Joel 02 -> Ellie 02 in Pittsburgh, where the companion wears 01) only works
// when the game streams the other outfit in fast enough: on a player's laptop
// her torso and arms stayed invisible, and Outfit=1 fixed it (Nexus,
// 2026-11-01). Known pairs are listed; others are learned from the companion's
// look in the first seconds after a spawn, with one checkpoint restart.
std::map<uint64_t, uint64_t> g_chapterOutfit;    // Joel's look -> the chapter Ellie's look (game thread)
std::atomic<uint64_t> g_spawnJoelLook{0};        // the Joel look of the last swapped spawn
std::atomic<uint64_t> g_spawnTick{0};
std::atomic<uint64_t> g_seenCompanionEllie{0};   // an Ellie look resolved for someone else after that spawn
std::atomic<int> g_playerLookOther{0}; // log budget for those
std::atomic<int> g_lookLogs{0};

// Frames the swap has waited for Ellie's packages; -1 = not waiting.
int g_swapWait = -1;
// Frames a toggle has waited for the level-set search; -1 = no swap pending.
int g_pendingSwap = -1;
// Frames since a restart while reporting package state; -1 = not reporting.
int g_reportFrames = -1;
std::atomic<bool> g_giveAfterSpawn{false};
// Frames since the mod restarted the checkpoint; -1 = none in progress. The
// toggle key is ignored meanwhile, and the next spawn shows "Swapped to ...".
int g_restarting = -1;
int g_restartSpawns = 0; // g_spawns when that restart began
bool g_greeted = false;  // "Playing as Ellie" shown this launch
// Whether a player exists, read once per frame at the start of Tick.
bool g_present = false;

uint64_t H(const std::string& s) { return dscan::Fnv1a64(s.c_str()); }

// Optional beeps (Sounds=1) on their own thread (Beep blocks): high = Ellie
// on, low = off, two = swap done, long low = something failed.
void Tone(int kind) {
    if (!g_sounds) return;
    std::thread([kind] {
        switch (kind) {
        case 0: Beep(660, 120); break;
        case 1: Beep(880, 120); break;
        case 2: Beep(990, 80); Sleep(60); Beep(990, 80); break;
        default: Beep(220, 400); break;
        }
    }).detach();
}

std::string Outfit(const char* who, int n, bool noBackpack) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "t1x-%s-costume-%02d%s", who, n, noBackpack ? "-no-backpack" : "");
    return buf;
}

template <size_t N>
bool Has(const int (&list)[N], int n) {
    for (int x : list)
        if (x == n) return true;
    return false;
}

template <size_t N>
int Closest(const int (&list)[N], int n) {
    int best = list[0];
    for (int x : list)
        if (x <= n) best = x;
    return best;
}

void BuildMaps() {
    using namespace engine;
    g_chapterOutfit[H("t1x-joel-costume-02")] = H("t1x-ellie-costume-01"); // Pittsburgh (logs, 2026-11-01)
    g_names[H(kJoel)] = kJoel;
    g_names[H(kEllie)] = kEllie;
    g_ellieBase = H(kEllie);
    g_toEllie[H(kJoel)] = H(kEllie);
    g_toJoel[H(kEllie)] = H(kJoel);
    for (int n : kJoelOutfits) {
        for (bool nb : {false, true}) {
            if (nb && !Has(kJoelNoBackpack, n)) continue;
            const int e = Closest(kEllieOutfits, n);
            const std::string joel = Outfit("joel", n, nb), ellie = Outfit("ellie", e, nb && Has(kEllieNoBackpack, e));
            g_names[H(joel)] = joel;
            g_toEllie[H(joel)] = H(ellie);
        }
    }
    for (int n : kEllieOutfits) {
        g_ellieList.push_back(Outfit("ellie", n, false));
        for (bool nb : {false, true}) {
            if (nb && !Has(kEllieNoBackpack, n)) continue;
            const int j = Closest(kJoelOutfits, n);
            const std::string ellie = Outfit("ellie", n, nb), joel = Outfit("joel", j, nb && Has(kJoelNoBackpack, j));
            g_names[H(ellie)] = ellie;
            g_toJoel[H(ellie)] = H(joel);
        }
    }
}

std::string NameOf(uint64_t id) {
    auto it = g_names.find(id);
    if (it != g_names.end()) return it->second;
    char hex[32];
    std::snprintf(hex, sizeof hex, "0x%016llX", static_cast<unsigned long long>(id));
    return hex;
}

// The chapter's Ellie outfit for Joel's look, or 0. Joel's no-backpack variant
// (after some cutscenes) counts as the same outfit: Ellie gets her matching
// no-backpack variant when there is one (same body parts).
uint64_t ChapterEllie(uint64_t joel) {
    auto ch = g_chapterOutfit.find(joel);
    if (ch != g_chapterOutfit.end()) return ch->second;
    const std::string name = NameOf(joel), nb = "-no-backpack";
    if (name.size() <= nb.size() || name.compare(name.size() - nb.size(), nb.size(), nb) != 0) return 0;
    ch = g_chapterOutfit.find(H(name.substr(0, name.size() - nb.size())));
    if (ch == g_chapterOutfit.end()) return 0;
    const uint64_t ellieNb = H(NameOf(ch->second) + nb);
    return g_names.count(ellieNb) ? ellieNb : ch->second;
}

// Every player spawn passes through here with the character to spawn.
void* __fastcall HkCore(void* a, void* b, void* c, void* d, uint64_t id, void* f) {
    uint64_t use = id;
    if (g_on) {
        auto it = g_toEllie.find(id);
        if (it != g_toEllie.end()) {
            use = (g_outfit >= 1 && g_outfit <= static_cast<int>(g_ellieList.size())) ? H(g_ellieList[g_outfit - 1]) : it->second;
            if (g_outfit == 0) {
                const uint64_t ch = ChapterEllie(id);
                if (ch) use = ch;
            }
        }
    }
    logx::Write("player spawn: %s%s%s", NameOf(id).c_str(), use != id ? " -> " : "", use != id ? NameOf(use).c_str() : "");
    const bool ellie = NameOf(use).rfind("t1x-ellie", 0) == 0;
    g_swappedTo = use != id && ellie ? 1 : 0;
    g_nativeEllie = use == id && ellie;
    ++g_spawns;
    g_lookLogs = 0;
    g_playerLook = g_swappedTo == 1 ? use : 0;
    g_playerLookOther = 0;
    g_spawnJoelLook = g_swappedTo == 1 ? id : 0;
    g_seenCompanionEllie = 0;
    g_spawnTick = GetTickCount64();
    g_playerSpawnThread = GetCurrentThreadId(); // looks resolved inside are the player's
    g_spawnThread = GetCurrentThreadId();
    void* r = oCore(a, b, c, d, use, f);
    g_playerSpawnThread = 0;
    return r;
}

// ------------------------------------------------- Joel as the companion
// While Ellie is the player, the story companion Ellie (same buddy: AI, script
// name, scripted moves, dialogue) gets Joel's look in the matching outfit.
// Looks resolved during the player spawn, and the player's outfit resolved again
// on a worker thread, are the player's and stay Ellie.
// Every Ellie/Joel look resolved is logged with its caller, so the log shows
// which code asked (companion spawner, cutscene actor, player, ...).
// Joel's look for the companion: the outfit Joel wears in this chapter (the
// one the player spawn started from, which the level loads), not the one with
// the matching number. In Pittsburgh the companion's Ellie outfit 01 became
// Joel's outfit 01, which this chapter doesn't use: on a player's laptop it
// never streamed in and the companion was invisible ("a walking flashlight",
// Nexus, 2026-11-01). The no-backpack variant is kept when both have one.
uint64_t ChapterJoel(uint64_t ellieLook, uint64_t byNumber) {
    const uint64_t joel = g_spawnJoelLook;
    if (!joel) return byNumber;
    const std::string name = NameOf(ellieLook), joelName = NameOf(joel);
    const std::string nb = "-no-backpack";
    const bool ellieNb = name.size() > nb.size() && name.compare(name.size() - nb.size(), nb.size(), nb) == 0;
    const bool joelNb = joelName.size() > nb.size() && joelName.compare(joelName.size() - nb.size(), nb.size(), nb) == 0;
    if (ellieNb == joelNb) return joel;
    const std::string other = ellieNb ? joelName + nb : joelName.substr(0, joelName.size() - nb.size());
    return g_names.count(H(other)) ? H(other) : joel;
}

bool JoelBuddyActive() { return g_joelBuddy && (g_on || g_swappedTo == 1) && !g_nativeEllie; }

void* __fastcall HkResolveLook(void* out, uint64_t look, void* c, void* d) {
    if (g_names.count(look)) {
        const bool spawnThread = g_playerSpawnThread == GetCurrentThreadId();
        // The player's outfit asked for again on a worker thread is the player's
        // (the game re-reads it there after the spawn). Asked for on the spawn
        // thread outside the spawn, it is the companion's: with Outfit=1 (or the
        // chapter's outfit) both wear the same one, and the companion still
        // becomes Joel.
        const bool player = spawnThread || (look == g_playerLook && GetCurrentThreadId() != g_spawnThread);
        if (player && !spawnThread && g_playerLookOther++ < 20)
            logx::Write("look: %s resolved outside the player spawn; kept as the player's (caller 0x%llX, thread %lu)",
                        NameOf(look).c_str(),
                        static_cast<unsigned long long>(reinterpret_cast<const uint8_t*>(_ReturnAddress()) - g_base),
                        GetCurrentThreadId());
        // Research (owner, 2026-11-01): when Joel picks up his backpack in a
        // scene, the game changes the player's look to Joel's without a spawn,
        // and she became Joel. Every Joel-outfit request while Ellie plays,
        // with its caller and thread, shows how that change asks for it
        // compared with the companion's own requests.
        static std::atomic<int> joelLogs{0};
        if (!spawnThread && g_swappedTo == 1 && g_toEllie.count(look) && joelLogs++ < 60)
            logx::Write("look: %s asked for while Ellie plays (caller 0x%llX, thread %lu%s)", NameOf(look).c_str(),
                        static_cast<unsigned long long>(reinterpret_cast<const uint8_t*>(_ReturnAddress()) - g_base),
                        GetCurrentThreadId(), GetCurrentThreadId() == g_spawnThread ? ", spawn thread" : "");
        uint64_t use = look;
        // The Skins menu's previews are left alone (turned into Joel they crashed
        // it). The same call also dresses characters right after a spawn (the
        // companion's skin): those are still swapped, or the Joel companion got
        // an Ellie skin and vanished (1.0.14-test, a player on Nexus).
        const bool skinCall = g_skinLookReturn && _ReturnAddress() == g_skinLookReturn;
        const bool skinMenu = skinCall && GetTickCount64() - g_spawnTick > 3000;
        // Log each skin-system request whose look differs from the last one, so a
        // menu's dozens of repeats don't hide the Ellie tab.
        static std::atomic<uint64_t> lastSkinLook{0};
        static std::atomic<int> skinLogs{0};
        if (skinCall && lastSkinLook.exchange(look) != look && skinLogs++ < 60)
            logx::Write("look: %s asked for by the skin system (%s)", NameOf(look).c_str(),
                        skinMenu ? "Skins menu: left alone" : "after a spawn: swapped like any other");
        const bool levelLoad = g_levelLookReturn && _ReturnAddress() == g_levelLookReturn;
        // The base look (t1x-ellie, no outfit number) is what the Skins menu's
        // preview character is built from; in the game every Ellie asks for a
        // numbered outfit. Turned into t1x-joel, the preview showed Joel, and
        // with an Ellie skin Joel's head on a backpack (a player on Nexus,
        // 2026-11-01), so it is never swapped.
        const bool baseLook = look == g_ellieBase;
        static std::atomic<int> baseLogs{0};
        if (baseLook && !player && baseLogs++ < 10)
            logx::Write("look: t1x-ellie (no outfit) asked for (caller 0x%llX): left alone",
                        static_cast<unsigned long long>(reinterpret_cast<const uint8_t*>(_ReturnAddress()) - g_base));
        if (!player && !skinMenu && !levelLoad && !baseLook && g_spawnJoelLook && g_toJoel.count(look) && GetTickCount64() - g_spawnTick < 5000) {
            uint64_t none = 0;
            g_seenCompanionEllie.compare_exchange_strong(none, look);
        }
        if (!player && !skinMenu && !levelLoad && !baseLook && JoelBuddyActive()) {
            auto it = g_toJoel.find(look);
            if (it != g_toJoel.end()) use = ChapterJoel(look, it->second);
        }
        if (g_lookLogs++ < 30) {
            const auto caller = reinterpret_cast<const uint8_t*>(_ReturnAddress()) - g_base;
            logx::Write("look: %s%s%s (caller 0x%llX%s)", NameOf(look).c_str(), use != look ? " -> " : "",
                        use != look ? NameOf(use).c_str() : "", static_cast<unsigned long long>(caller),
                        spawnThread ? ", player spawn" : player ? ", player's look" : skinMenu ? ", Skins menu" : levelLoad ? ", level load: left alone" : "");
        }
        look = use;
    }
    return oResolveLook(out, look, c, d);
}

// ---------------------------------------------------------------- packages
// Ellie's packages that the game actually has (see engine_defs.h), alone and
// with Joel's companion packages, with their name hashes.
struct Package {
    const char* name;
    uint64_t hash;
    bool optional; // never loads in Joel's chapters; the swap does not wait for it
};
std::vector<Package> g_elliePackages, g_ellieJoelBuddyPackages;

// Not in any set the patch adds (seen in every test of w1z's menu and this mod:
// 50 of 51 loaded, only this one missing). Waiting for it cost ~5 s per swap.
constexpr const char* kOptionalPackages[] = {"anim-ellie-player-mm-wade"};

// Ellie's outfits are made of parts (ellie-costume-NN-torso, -legs, bandages)
// that no level set lists: the look system streams them in when a character
// puts the outfit on, and in Joel's chapters that doesn't always happen in time
// (a player on Nexus with Outfit=8, 2026-11-01: arms missing, then everything
// but her head). With Outfit= set, the patch also loads that outfit's parts and
// the swap waits for them. Every Ellie spawn logs whether her outfit's parts
// are loaded, so the chapter outfits (Outfit=0) can be checked too.
constexpr const char* kOutfitParts[] = {"torso", "torso-no-backpack", "legs", "bandage-bicep",
                                        "bandage-forearm", "bandages-bicep", "bandages-forearm"};
std::vector<std::string> g_outfitPartNames;  // the chosen outfit's parts; reserved once, c_str() stays put
std::vector<Package> g_outfitParts;          // the same, with hashes
std::vector<std::string> g_allOutfitParts;   // every Ellie outfit part the game has (for the spawn log)

// Keeps the names listed in the game's actor97.txt.
template <size_t N>
void FilterPackages(const char* const (&list)[N], const std::vector<std::string>& actors, std::vector<Package>& out,
                    const char* who) {
    for (const char* name : list) {
        if (std::find(actors.begin(), actors.end(), name) == actors.end()) {
            logx::Write("ellie: %s package %s is not in actor97.txt; skipped", who, name);
            continue;
        }
        bool optional = false;
        for (const char* o : kOptionalPackages) optional = optional || std::strcmp(o, name) == 0;
        out.push_back({name, dscan::Fnv1a64(name), optional});
    }
}

bool LoadPackageLists() {
    char exe[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return false;
    std::string path(exe);
    path = path.substr(0, path.find_last_of("\\/") + 1) + "build\\pc\\main\\actor97.txt";
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) {
        logx::Write("ellie: cannot read %s", path.c_str());
        return false;
    }
    std::vector<std::string> actors;
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (!s.empty()) actors.push_back(s);
    }
    std::fclose(f);
    FilterPackages(engine::kEllieOnlyPackages, actors, g_elliePackages, "Ellie");
    FilterPackages(engine::kJoelBuddyPackages, actors, g_ellieJoelBuddyPackages, "Joel companion");
    g_ellieJoelBuddyPackages.insert(g_ellieJoelBuddyPackages.begin(), g_elliePackages.begin(), g_elliePackages.end());
    for (const auto& a : actors)
        for (const char* part : kOutfitParts)
            if (a.rfind("ellie-costume-", 0) == 0 && a.size() == 17 + std::strlen(part) && a.compare(17, std::string::npos, part) == 0)
                g_allOutfitParts.push_back(a);
    constexpr int kOutfitCount = static_cast<int>(sizeof engine::kEllieOutfits / sizeof engine::kEllieOutfits[0]);
    if (g_outfit >= 1 && g_outfit <= kOutfitCount) {
        g_outfitPartNames.reserve(std::size(kOutfitParts));
        std::string list;
        for (const char* part : kOutfitParts) {
            char name[64];
            std::snprintf(name, sizeof name, "ellie-costume-%02d-%s", engine::kEllieOutfits[g_outfit - 1], part);
            if (std::find(actors.begin(), actors.end(), name) == actors.end()) continue;
            g_outfitPartNames.push_back(name);
            list += (list.empty() ? "" : ", ") + g_outfitPartNames.back();
        }
        for (const auto& part : g_outfitPartNames) g_outfitParts.push_back({part.c_str(), dscan::Fnv1a64(part.c_str()), false});
        g_elliePackages.insert(g_elliePackages.end(), g_outfitParts.begin(), g_outfitParts.end());
        g_ellieJoelBuddyPackages.insert(g_ellieJoelBuddyPackages.end(), g_outfitParts.begin(), g_outfitParts.end());
        logx::Write("ellie: Outfit=%d also loads its parts: %s", g_outfit, list.empty() ? "none found" : list.c_str());
    }
    logx::Write("ellie: %zu Ellie player packages available (%zu actors listed)", g_elliePackages.size(), actors.size());
    return !g_elliePackages.empty();
}

const std::vector<Package>& NeededPackages() { return g_joelBuddy ? g_ellieJoelBuddyPackages : g_elliePackages; }

// Game thread only. How many of the needed packages are fully loaded, and
// (if asked) how many of the ones the swap waits for are still missing.
int LoadedCount(size_t& total, int* requiredMissing = nullptr) {
    const auto& list = NeededPackages();
    void* mgr = *g_packageMgr;
    total = list.size();
    if (requiredMissing) *requiredMissing = static_cast<int>(total);
    if (!mgr) return 0;
    int loaded = 0, missing = 0;
    for (const auto& p : list) {
        if (g_loaded(mgr, p.hash)) ++loaded;
        else if (!p.optional) ++missing;
    }
    if (requiredMissing) *requiredMissing = missing;
    return loaded;
}

// Game thread only. True when a part of the chosen outfit (Outfit=) is not loaded.
bool OutfitPartsMissing() {
    void* mgr = *g_packageMgr;
    for (const auto& p : g_outfitParts)
        if (!mgr || !g_loaded(mgr, p.hash)) return true;
    return false;
}

// Game thread only. Logs which parts of the player's outfit are loaded.
void LogPlayerOutfit(uint64_t look) {
    void* mgr = *g_packageMgr;
    const std::string name = NameOf(look);
    if (!mgr || name.rfind("t1x-ellie-costume-", 0) != 0 || name.size() < 20) return;
    const std::string prefix = "ellie-costume-" + name.substr(18, 2) + "-";
    std::string loaded, missing;
    for (const auto& a : g_allOutfitParts) {
        if (a.rfind(prefix, 0) != 0) continue;
        std::string& to = g_loaded(mgr, dscan::Fnv1a64(a.c_str())) ? loaded : missing;
        to += (to.empty() ? "" : ", ") + a.substr(prefix.size());
    }
    logx::Write("swap: her outfit %s: loaded [%s], not loaded [%s]", name.c_str(), loaded.c_str(), missing.c_str());
}

// Game thread only. Logs the needed packages that are not loaded.
void LogMissing() {
    void* mgr = *g_packageMgr;
    if (!mgr) return;
    for (const auto& p : NeededPackages())
        if (!g_loaded(mgr, p.hash)) logx::Write("swap: not loaded: %s%s", p.name, p.optional ? " (never loads here; not needed)" : "");
}

// ---------------------------------------------------------------- level sets
// Packages can only be loaded safely by the game itself (loading them from
// the mod runs texture jobs outside the game's job system and trips an
// assert). So while Ellie is on, the player's level set for the chapter
// (joel-player-all) also lists Ellie's sets, and the game's own want-load
// rebuild streams those packages in.
//
// The level sets live in memory as DC data (level-sets-common.bin): a table
// of {name hash, type hash, record} entries; a record is {name string, name
// hash, body}, and body+0x40 is the level-sets array {pointer, count |
// 0xFFFFFFFF << 32} of pointers to {set name hash, 0}.
constexpr uint64_t kLevelSetType = 0xA84077CAA2495B60;
constexpr size_t kLevelSetsField = 0x40;
constexpr size_t kActorsField = 0x20;

bool ReadMem(uint64_t addr, void* out, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(addr), out, n, &got) && got == n;
}

struct SetRef {
    uint64_t hash, zero;
};

// An actor (package) entry in a level set: {name, name hash}.
struct ActorRef {
    const char* name;
    uint64_t hash;
};

struct SetPatch {
    const char* set;                      // the chapter's player set
    std::vector<const char*> add;         // sets to add to it
    std::vector<const char*> actors = {}; // single packages to add to it
    uint64_t field = 0;                   // address of {pointer, count} in memory
    uint64_t origPtr = 0, origCount = 0;
    std::vector<SetRef> refs;    // our entries (built once per apply)
    std::vector<uint64_t> array; // original entries + pointers to refs
    uint64_t origActorPtr = 0, origActorCount = 0;
    std::vector<ActorRef> actorRefs;
    std::vector<uint64_t> actorArray;
    bool applied = false;
};

std::vector<SetPatch> g_patches;
std::atomic<bool> g_setsFound{false}, g_setsSearching{false};

// Validates a candidate record for `name`; returns the level-sets field address.
uint64_t CheckRecord(uint64_t rec, const char* name) {
    uint64_t r[3];
    if (!ReadMem(rec, r, sizeof r) || r[1] != dscan::Fnv1a64(name)) return 0;
    char str[64] = {};
    const size_t len = std::strlen(name) + 1;
    if (len > sizeof str || !ReadMem(r[0], str, len) || std::memcmp(str, name, len) != 0) return 0;
    uint64_t f[2];
    const uint64_t field = r[2] + kLevelSetsField;
    if (!ReadMem(field, f, sizeof f) || (f[1] >> 32) != 0xFFFFFFFF) return 0;
    const uint32_t n = static_cast<uint32_t>(f[1]);
    if (n == 0 || n > 64) return 0;
    for (uint32_t i = 0; i < n; ++i) { // every entry points at {hash, 0}
        uint64_t e, ref[2];
        if (!ReadMem(f[0] + 8 * i, &e, 8) || !ReadMem(e, ref, sizeof ref) || ref[1] != 0) return 0;
    }
    return field;
}

// Searches the game's memory for the player level-set records (a second or
// two; run off the game thread). Reads go through ReadProcessMemory, so
// memory the game frees meanwhile cannot crash the scan.
void FindLevelSets() {
    std::vector<uint8_t> buf(1 << 20);
    std::vector<uint64_t> fields(g_patches.size(), 0);
    MEMORY_BASIC_INFORMATION mbi{};
    size_t found = 0;
    const DWORD start = GetTickCount();
    for (uint64_t a = 0x10000; found < fields.size() && VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof mbi);
         a = reinterpret_cast<uint64_t>(mbi.BaseAddress) + mbi.RegionSize) {
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || (mbi.Protect & 0xFF) != PAGE_READWRITE) continue;
        const uint64_t base = reinterpret_cast<uint64_t>(mbi.BaseAddress), end = base + mbi.RegionSize;
        for (uint64_t at = base; at + 24 <= end; at += buf.size() - 24) {
            const size_t n = static_cast<size_t>((std::min<uint64_t>)(buf.size(), end - at));
            if (!ReadMem(at, buf.data(), n)) break;
            for (size_t p = 0; p < fields.size(); ++p) {
                if (fields[p]) continue;
                const char* set = g_patches[p].set;
                const uint64_t key[2] = {dscan::Fnv1a64(set), kLevelSetType};
                for (size_t i = 0; i + 24 <= n; i += 8) { // DC tables are 8-byte aligned
                    if (std::memcmp(buf.data() + i, key, 16) != 0) continue;
                    uint64_t rec;
                    std::memcpy(&rec, buf.data() + i + 16, 8);
                    if (const uint64_t field = CheckRecord(rec, set)) {
                        fields[p] = field;
                        ++found;
                        break;
                    }
                }
            }
            if (n < buf.size()) break;
        }
    }
    for (size_t p = 0; p < fields.size(); ++p)
        logx::Write("ellie: level set %s %s", g_patches[p].set, fields[p] ? "found" : "NOT found");
    logx::Write("ellie: level-set search took %lu ms", GetTickCount() - start);
    if (found != fields.size()) return;
    // The patches are only touched on the game thread.
    gameloop::RunOnGameThread([fields] {
        for (size_t p = 0; p < fields.size(); ++p)
            if (!g_patches[p].applied) g_patches[p].field = fields[p];
        g_setsFound = true;
    });
}

// Game thread only. Points each set's level-sets array at original + added.
void ApplyPatches() {
    for (auto& p : g_patches) {
        if (p.applied || !p.field) continue;
        uint64_t f[2];
        if (!ReadMem(p.field, f, sizeof f)) continue;
        const uint32_t n = static_cast<uint32_t>(f[1]);
        p.origPtr = f[0];
        p.origCount = f[1];
        std::vector<const char*> add = p.add, actors = p.actors;
        for (const auto& part : g_outfitPartNames) actors.push_back(part.c_str());
        if (g_joelBuddy) {
            add.insert(add.end(), std::begin(engine::kJoelBuddySets), std::end(engine::kJoelBuddySets));
            actors.insert(actors.end(), std::begin(engine::kJoelBuddyPackages), std::end(engine::kJoelBuddyPackages));
        }
        p.refs.clear();
        p.refs.reserve(add.size());
        for (const char* s : add) p.refs.push_back({dscan::Fnv1a64(s), 0});
        p.array.assign(n, 0);
        if (!ReadMem(f[0], p.array.data(), 8 * n)) continue;
        for (auto& r : p.refs) p.array.push_back(reinterpret_cast<uint64_t>(&r));
        // Actors array (body+0x20, 0x20 before the level-sets array): same layout.
        const uint64_t actorField = p.field - kLevelSetsField + kActorsField;
        uint64_t af[2];
        bool actorsOk = false;
        if (!actors.empty() && ReadMem(actorField, af, sizeof af) && (af[1] >> 32) == 0xFFFFFFFF) {
            const uint32_t an = static_cast<uint32_t>(af[1]);
            p.origActorPtr = af[0];
            p.origActorCount = af[1];
            p.actorRefs.clear();
            p.actorRefs.reserve(actors.size()); // the array points into it
            for (const char* a : actors) p.actorRefs.push_back({a, dscan::Fnv1a64(a)});
            p.actorArray.assign(an, 0);
            if (an == 0 || ReadMem(af[0], p.actorArray.data(), 8 * an)) {
                for (auto& r : p.actorRefs) p.actorArray.push_back(reinterpret_cast<uint64_t>(&r));
                actorsOk = true;
            } else {
                p.actorArray.clear();
            }
        }
        auto* slot = reinterpret_cast<volatile uint64_t*>(p.field);
        slot[0] = reinterpret_cast<uint64_t>(p.array.data()); // pointer first: an old count stays in bounds
        slot[1] = (f[1] & 0xFFFFFFFF00000000ull) | p.array.size();
        if (actorsOk) {
            auto* aslot = reinterpret_cast<volatile uint64_t*>(actorField);
            aslot[0] = reinterpret_cast<uint64_t>(p.actorArray.data());
            aslot[1] = (af[1] & 0xFFFFFFFF00000000ull) | p.actorArray.size();
        }
        p.applied = true;
        logx::Write("ellie: level set %s now also loads %zu more set(s) and %zu package(s)", p.set, add.size(),
                    actorsOk ? actors.size() : 0);
    }
}

void RemovePatches() {
    for (auto& p : g_patches) {
        if (!p.applied) continue;
        auto* slot = reinterpret_cast<volatile uint64_t*>(p.field);
        slot[1] = p.origCount; // count first: never past the end of the original array
        slot[0] = p.origPtr;
        if (!p.actorArray.empty()) {
            auto* aslot = reinterpret_cast<volatile uint64_t*>(p.field - kLevelSetsField + kActorsField);
            aslot[1] = p.origActorCount;
            aslot[0] = p.origActorPtr;
            p.actorArray.clear();
        }
        p.applied = false;
        logx::Write("ellie: level set %s restored", p.set);
    }
}

// ------------------------------------------------------------- 9mm hold
// Root cause (tested in game): Joel's 9mm has an Ellie overlay table in
// player-specific-weapon-overlays/t1x-guns/t1x-pistol-9mm (12 entries). All of
// its entries are animations made for the scripted scenes where she holds his
// gun, so in normal play the gun tilts ~70 degrees up in her hand. Joel's
// revolver table is made for Ellie too, and with it the 9mm sits level and
// recoils on time. Its hold pose is made for a bigger gun, so the 9mm uses the
// mod's own copy of the revolver table whose two hold entries take Ellie's
// own pistol hold (from t1x-pistol-ellie): the gun points straight (tested in
// game) and the rest stays the revolver's. Only the 9mm's reference changes;
// both game tables stay as they are, and it is restored for Joel.
//
// Tables are found by their first entries; each is confirmed by its {table,
// count} reference at a fixed distance before it (same DC file layout).
constexpr uint64_t k9mmOverlayFirst[2] = {0x3A44AABBD653F271, 0x999E2C69E65BC987};
constexpr uint64_t kGripAim = 0x8C1A665995B5EE09; // pistol-beretta-grip-friendly-aim-part, entry 6 (+0x180)
constexpr uint64_t k9mmOverlayCount = 12;
constexpr uint64_t kRevolverOverlayFirst[2] = {0x3A44AABBD653F271, 0x19E1B5667FD820F7};
constexpr uint64_t kRevolverSecondKey = 0x8D99EFE7C9519C6E; // entry 1 (+0x40)
constexpr uint64_t kRevolverOverlayCount = 55;
constexpr uint64_t kHoldKeys[2] = {0x3A44AABBD653F271, 0x8D99EFE7C9519C6E}; // revolver entries 0 and 1
constexpr uint64_t kRevolverHold = 0x19E1B5667FD820F7, kEllieHold = 0x5A3555FEAA9DAA3B;
constexpr uint64_t kSceneHold = 0x999E2C69E65BC987; // the 9mm table's scripted-scene hold (the tilt)
uint64_t g_9mmOverlayCount = 0; // address of the 9mm's {table, count} count field, 0 = not found
uint64_t g_9mmTable = 0, g_revolverTable = 0;
std::atomic<bool> g_gunSearching{false};
int g_gunSearches = 0;

// Joel's bow, put away, lies across Ellie's backpack (Joel's holster spot). Its
// weapon-art record (weapon-art.bin, ProcessWeaponBow at +0x38, name at +0x30)
// has settings pointers at +0x158 and +0x180 and a hash at +0x198; the +0x180
// block holds the holster setting (+0x40: s_bow-slot1-holster on Joel's bow,
// s_ellie-bow-slot1-holster on Ellie's own t1x-bow-ellie). In w1z's menu's live
// test, giving Joel's bow all three of Ellie's values put it vertically on her
// back after a weapon switch (2026-10-30); the crouch and invisible bow when
// aiming came afterwards, with her bow's overlay table, which isn't used here.
// Changing only the holster key inside the block did nothing (1.0.5-test).
// Modes: 0 = Joel's own, 1 = Ellie's +0x180 block, 2 = all three of Ellie's.
constexpr uint64_t kProcessWeaponBow = 0x63CB48375E6E92D6;
constexpr uint64_t kJoelBowHolster = 0x5062ADC8205A58B9;  // s_bow-slot1-holster
constexpr uint64_t kEllieBowHolster = 0xAB0ABC3E74FAA491; // s_ellie-bow-slot1-holster
constexpr uint32_t kBowFields[3] = {0x180, 0x158, 0x198};
constexpr int kBowModes = 3;
const char* const kBowModeNames[kBowModes] = {"Joel's own", "Ellie's holster block", "all of Ellie's bow settings"};
int g_bowMode = 2; // wanted while Ellie plays (BowOnBack=1: 2, 0: Joel's own)
int g_bowApplied = 0;        // in memory now
uint64_t g_joelBow = 0, g_ellieBow = 0; // the two records, 0 = not found
uint64_t g_joelBowOrig[3] = {}, g_ellieBowVals[3] = {};

// Which table the 9mm uses while Ellie plays (Fix9mm=). 1.0 used mode 1: the
// gun points straight, but when it runs dry the game's automatic weapon switch
// leaves her arms out with no animation until she aims (owner, 2026-10-30;
// gone with mode 0, which tilts the gun up again). The other modes mix the
// 9mm's own entries (which switch fine) with the revolver's (which hold it
// straight) to find one that does both. None did: modes 2-6 all tilted the gun
// up (owner, 2026-10-30), so releases offer only 0 and 1 and keep 1.
constexpr int kGunModes = 7;
const char* const kGunModeNames[kGunModes] = {
    "the game's own (tilts up)",
    "1.0: revolver copy + Ellie's hold",
    "revolver copy + the 9mm's own extras (no scene grip)",
    "9mm's own + the revolver's aim values (no scene grip)",
    "revolver copy + all the 9mm's own extras",
    "9mm's own + the revolver's aim values",
    "9mm's own with Ellie's hold",
};
int g_gunMode = 1;          // wanted while Ellie plays
int g_gunApplied = 0;       // in memory now (0 = the game's own reference)
uint64_t g_gunAppliedRef[2] = {}; // {table, count} written, to check nothing else changed it
std::vector<uint64_t> g_gunTables[kGunModes]; // built once; their memory never moves afterwards

using Entry = std::array<uint64_t, 8>; // {key, overlay set, ...}: one 0x40-byte table entry

// Game thread only. Builds every mode's table from the two game tables.
bool BuildGunTables() {
    if (!g_gunTables[1].empty()) return true;
    std::vector<Entry> nine(k9mmOverlayCount), rev(kRevolverOverlayCount);
    if (!ReadMem(g_9mmTable, nine.data(), nine.size() * 0x40) || !ReadMem(g_revolverTable, rev.data(), rev.size() * 0x40) ||
        rev[0][0] != kHoldKeys[0] || rev[0][1] != kRevolverHold || rev[1][0] != kHoldKeys[1] || rev[1][1] != kRevolverHold ||
        nine[0][0] != k9mmOverlayFirst[0] || nine[0][1] != kSceneHold) {
        logx::Write("ellie: 9mm or revolver table changed; 9mm left as it is");
        return false;
    }
    std::vector<Entry> revMix = rev; // mode 1: 1.0's table
    revMix[0][1] = revMix[1][1] = kEllieHold;
    auto find = [](const std::vector<Entry>& t, uint64_t key) -> const Entry* {
        for (const auto& e : t)
            if (e[0] == key) return &e;
        return nullptr;
    };
    // The 9mm's own entries the revolver table has no key for.
    std::vector<Entry> extras, extrasNoGrip;
    for (const auto& e : nine)
        if (!find(rev, e[0])) {
            extras.push_back(e);
            if (e[0] != kGripAim) extrasNoGrip.push_back(e);
        }
    // The 9mm's own table, values for keys the revolver has taken from 1.0's
    // table (hold -> Ellie's hold), the scene hold anywhere else -> Ellie's.
    std::vector<Entry> nineRev, nineRevNoGrip, nineHold = nine;
    for (const auto& e : nine) {
        Entry x = e;
        if (const Entry* r = find(revMix, e[0])) x[1] = (*r)[1];
        if (x[1] == kSceneHold) x[1] = kEllieHold;
        nineRev.push_back(x);
        if (e[0] != kGripAim) nineRevNoGrip.push_back(x);
    }
    for (auto& e : nineHold)
        if (e[1] == kSceneHold) e[1] = kEllieHold;
    auto flat = [](std::vector<Entry> a, const std::vector<Entry>& b = {}) {
        a.insert(a.end(), b.begin(), b.end());
        std::vector<uint64_t> out;
        for (const auto& e : a) out.insert(out.end(), e.begin(), e.end());
        return out;
    };
    g_gunTables[1] = flat(revMix);
    g_gunTables[2] = flat(revMix, extrasNoGrip);
    g_gunTables[3] = flat(nineRevNoGrip);
    g_gunTables[4] = flat(revMix, extras);
    g_gunTables[5] = flat(nineRev);
    g_gunTables[6] = flat(nineHold);
    logx::Write("ellie: 9mm tables built (%zu of the 9mm's own entries are not in the revolver's)", extras.size());
    return true;
}

// The ProcessWeaponBow record named `name` whose type hash is at `type`, if its
// +0x180 block holds `holster`; else 0.
uint64_t BowRecord(uint64_t type, const char* name, uint64_t holster) {
    uint64_t namePtr = 0, block = 0, value = 0;
    char str[16] = {};
    const size_t len = std::strlen(name) + 1;
    if (len > sizeof str || !ReadMem(type - 8, &namePtr, 8) || !ReadMem(namePtr, str, len) ||
        std::memcmp(str, name, len) != 0 || !ReadMem(type - 0x38 + 0x180, &block, 8) || !block ||
        !ReadMem(block + 0x40, &value, 8) || value != holster)
        return 0;
    return type - 0x38;
}

void FindGunOverlay() {
    uint64_t found = 0, table9 = 0, revolver = 0, joelBow = 0, ellieBow = 0;
    std::vector<uint8_t> buf(1 << 20);
    MEMORY_BASIC_INFORMATION mbi{};
    const DWORD start = GetTickCount();
    auto done = [&] { return found && revolver && joelBow && ellieBow; };
    for (uint64_t a = 0x10000; !done() && VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof mbi);
         a = reinterpret_cast<uint64_t>(mbi.BaseAddress) + mbi.RegionSize) {
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || (mbi.Protect & 0xFF) != PAGE_READWRITE) continue;
        const uint64_t base = reinterpret_cast<uint64_t>(mbi.BaseAddress), end = base + mbi.RegionSize;
        for (uint64_t at = base; at + 16 <= end && !done(); at += buf.size() - 16) {
            const size_t n = static_cast<size_t>((std::min<uint64_t>)(buf.size(), end - at));
            if (!ReadMem(at, buf.data(), n)) break;
            for (size_t i = 0; i + 16 <= n; i += 8) {
                const uint64_t table = at + i;
                uint64_t key2 = 0, ref[2] = {}, word = 0;
                std::memcpy(&word, buf.data() + i, 8);
                if (word == kProcessWeaponBow && (!joelBow || !ellieBow)) {
                    if (!joelBow) joelBow = BowRecord(table, "t1x-bow", kJoelBowHolster);
                    if (!ellieBow) ellieBow = BowRecord(table, "t1x-bow-ellie", kEllieBowHolster);
                    continue;
                }
                if (!found && std::memcmp(buf.data() + i, k9mmOverlayFirst, 16) == 0 && ReadMem(table + 0x180, &key2, 8) &&
                    key2 == kGripAim && ReadMem(table - 0xE8, ref, sizeof ref) && ref[0] == table &&
                    ref[1] == k9mmOverlayCount) {
                    found = table - 0xE0;
                    table9 = table;
                } else if (!revolver && std::memcmp(buf.data() + i, kRevolverOverlayFirst, 16) == 0 &&
                           ReadMem(table + 0x40, &key2, 8) && key2 == kRevolverSecondKey &&
                           ReadMem(table - 0x220, ref, sizeof ref) && ref[0] == table && ref[1] == kRevolverOverlayCount) {
                    revolver = table;
                }
            }
            if (n < buf.size()) break;
        }
    }
    logx::Write("ellie: 9mm Ellie overlay %s, revolver overlay %s, bow records %s (%lu ms)", found ? "found" : "NOT found",
                revolver ? "found" : "NOT found",
                joelBow && ellieBow ? "found" : joelBow ? "NOT found (Ellie's bow)" : "NOT found",
                GetTickCount() - start);
    gameloop::RunOnGameThread([found, table9, revolver, joelBow, ellieBow] {
        if (!g_gunApplied && !g_9mmOverlayCount && found && revolver) {
            g_9mmOverlayCount = found, g_9mmTable = table9, g_revolverTable = revolver;
            g_gunAppliedRef[0] = table9, g_gunAppliedRef[1] = k9mmOverlayCount;
        }
        if (!g_joelBow && joelBow && ellieBow) {
            for (int f = 0; f < 3; ++f)
                if (!ReadMem(joelBow + kBowFields[f], &g_joelBowOrig[f], 8) || !ReadMem(ellieBow + kBowFields[f], &g_ellieBowVals[f], 8))
                    return void(g_gunSearching = false);
            g_joelBow = joelBow, g_ellieBow = ellieBow;
        }
        g_gunSearching = false;
    });
}

// Game thread only. Joel's bow gets Ellie's bow settings (g_bowMode) while
// Ellie plays; Joel's own values otherwise.
void SyncBowHolster(bool ellie) {
    const int want = ellie ? g_bowMode : 0;
    if (want == g_bowApplied || !g_joelBow) return;
    auto* rec = reinterpret_cast<volatile uint64_t*>(g_joelBow);
    for (int f = 0; f < 3; ++f) { // each field must still hold what was written last
        const bool mine = g_bowApplied == 2 || (g_bowApplied == 1 && f == 0);
        if (rec[kBowFields[f] / 8] != (mine ? g_ellieBowVals[f] : g_joelBowOrig[f])) {
            logx::Write("ellie: bow settings changed; left alone");
            g_joelBow = 0;
            return;
        }
    }
    for (int f = 0; f < 3; ++f) {
        const bool ellies = want == 2 || (want == 1 && f == 0);
        rec[kBowFields[f] / 8] = ellies ? g_ellieBowVals[f] : g_joelBowOrig[f];
    }
    g_bowApplied = want;
    logx::Write("ellie: Joel's bow uses %s", kBowModeNames[want]);
}

// Searches for the 9mm's overlay in the background (its data may only be in
// memory once a game is loaded, so this is retried on Ellie spawns).
void StartGunSearch() {
    if ((g_9mmOverlayCount || (!g_gunMode && !PE_TEST_9MM)) && g_joelBow) return;
    if (g_gunSearches >= 5 || g_gunSearching.exchange(true)) return;
    ++g_gunSearches;
    std::thread(&FindGunOverlay).detach();
}

// Game thread only. Points the 9mm at the table for the current mode (Ellie
// playing) or back at its own table.
void SyncGunOverlay(bool ellie) {
    const int want = ellie ? g_gunMode : 0;
    if (want == g_gunApplied || !g_9mmOverlayCount) return;
    if (want && !BuildGunTables()) {
        g_9mmOverlayCount = 0;
        g_gunSearches = 5;
        return;
    }
    // The {table, count} reference: count at g_9mmOverlayCount, pointer just before.
    auto* ref = reinterpret_cast<volatile uint64_t*>(g_9mmOverlayCount - 8);
    uint64_t cur[2] = {};
    if (!ReadMem(g_9mmOverlayCount - 8, cur, sizeof cur) || cur[0] != g_gunAppliedRef[0] || cur[1] != g_gunAppliedRef[1]) {
        logx::Write("ellie: 9mm Ellie overlay changed; left alone");
        g_9mmOverlayCount = 0;
        g_gunSearches = 5; // the data moved: stop looking
        return;
    }
    const uint64_t table = want ? reinterpret_cast<uint64_t>(g_gunTables[want].data()) : g_9mmTable;
    const uint64_t count = want ? g_gunTables[want].size() / 8 : k9mmOverlayCount;
    ref[1] = 0; // count first: never a count past the end of either table
    ref[0] = table;
    ref[1] = count;
    g_gunAppliedRef[0] = table, g_gunAppliedRef[1] = count;
    g_gunApplied = want;
    logx::Write("ellie: 9mm uses table %d: %s (%llu entries)", want, kGunModeNames[want], static_cast<unsigned long long>(count));
}

// ------------------------------------------------------------- crafting
// The crafting controller's "allowed" check only lets an Ellie player craft
// in Winter (lakeside) or when a game flag is set (see
// dscan::FindEllieCraftFlag). On while Ellie is the player, restored after.
volatile uint8_t* g_ellieCraftFlag = nullptr;
uint8_t g_ellieCraftOrig = 0;
bool g_ellieCraftOn = false;

// Game thread only.
void SyncCraftFlag(bool ellie) {
    if (ellie == g_ellieCraftOn || !g_ellieCraftFlag) return;
    if (ellie) {
        g_ellieCraftOrig = *g_ellieCraftFlag;
        *g_ellieCraftFlag = 1;
    } else {
        *g_ellieCraftFlag = g_ellieCraftOrig;
    }
    g_ellieCraftOn = ellie;
    logx::Write("ellie: crafting for Ellie outside Winter %s", ellie ? "on" : "restored");
}

// Swapped-in Ellie starts with an inventory that knows no crafting recipes in
// Joel's chapters, so the crafting menu has nothing to show. Game thread.
void GiveRecipes() {
    if (!g_allRecipes) return;
    if (gameloop::RunDevCallback(g_allRecipes, dscan::Fnv1a64(engine::kAllRecipesData)))
        logx::Write("swap: taught Ellie the crafting recipes (All Recipes)");
}

// ------------------------------------------------------------- messages
bool g_notify = true; // Notifications=1: on-screen messages

void Say(const char* text) {
    if (g_notify) overlay::Toast(text);
    else logx::Write("message: %s", text);
}
void Busy(const std::string& text, float progress = -1.f) {
    if (g_notify) overlay::Status(text, progress);
}
void Done(const char* text) {
    overlay::ClearStatus();
    Say(text);
}

// ------------------------------------------------------------- the swap

// Game thread only. Applies the patches for the current setting. A patch
// stays while the player it loads is still the one playing (the game re-reads
// level sets on its own, and unloading her animations under her would freeze
// or crash the game); it is removed after the next normal spawn.
void SyncPatches() {
    const bool ellie = g_on || g_swappedTo == 1;
    SyncGunOverlay(ellie);
    SyncBowHolster(ellie);
    SyncCraftFlag(ellie);
    if (!g_setsFound) return;
    if (g_on) ApplyPatches();
    else if (g_swappedTo != 1) RemovePatches();
}

// Finds the level sets once (in the background), then applies the patches.
void PrepareSwap() {
    if (g_setsFound) {
        gameloop::RunOnGameThread(&SyncPatches);
        return;
    }
    if (g_setsSearching.exchange(true)) return;
    std::thread([] {
        FindLevelSets();
        gameloop::RunOnGameThread([] {
            g_setsSearching = false;
            SyncPatches();
            if (g_setsFound && g_present) StartGunSearch(); // after, so it never delays the swap
        });
    }).detach();
}

// Game thread only. Makes the game rebuild its want-load list from the
// current level set, so the sets the patches add start streaming in.
bool RequestPackages() {
    void* mgr = *g_levelMgr;
    if (!mgr) return false;
    g_rebuildWants(mgr);
    logx::Write("swap: asked the game to stream in Ellie's packages");
    return true;
}

void DoRestart() {
    logx::Write("swap: restarting checkpoint (%s)", g_on ? "as Ellie" : "as the story character");
    Busy("Restarting checkpoint...");
    g_reportFrames = g_on ? 0 : -1; // report how many are loaded after the restart
    g_restarting = 0;
    g_restartSpawns = g_spawns;
    g_restart(false, false);
}

// Game thread only. Swaps now: streams Ellie's animations in first (a
// checkpoint restart never loads them, and without them she cannot move),
// then restarts from the last checkpoint.
void SwapNow() {
    SyncPatches();
    if (g_on && RequestPackages()) {
        g_swapWait = 0;
        Busy("Loading Ellie's animations...", 0.f);
        return;
    }
    DoRestart();
}

// The toggle key. Game thread only.
void OnToggleKey() {
    if (g_swapWait >= 0 || g_pendingSwap >= 0 || g_restarting >= 0) {
        logx::Write("key: a swap is still in progress; ignored");
        Say("Still swapping - one moment");
        Tone(3);
        return;
    }
    g_on = !g_on;
    settings::SetBool("PlayAsEllie", g_on);
    logx::Write("key: Play as Ellie %s", g_on ? "ON" : "OFF");
    Tone(g_on ? 1 : 0);
    if (!g_present) {
        logx::Write("key: no game loaded; applies when you load one");
        Say(g_on ? "Play as Ellie: ON (applies when you load a game)" : "Play as Ellie: OFF (applies when you load a game)");
        PrepareSwap();
        return;
    }
    if (g_nativeEllie) { // one of Ellie's own chapters: nothing to swap
        logx::Write("key: this chapter's player is already Ellie; no restart");
        Say(g_on ? "Play as Ellie: ON (this chapter is already Ellie's)" : "Play as Ellie: OFF (this chapter is Ellie's)");
        SyncPatches();
        return;
    }
    if (g_on && !g_setsFound) {
        g_pendingSwap = 0;
        Busy("Preparing Ellie...");
        PrepareSwap();
        return;
    }
    if (!g_on) Busy("Swapping to Joel...");
    SwapNow();
}

bool GameInForeground() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

// A swapped Ellie that spawned without her animations (a new chapter started,
// or the patch was not in yet when the level loaded) gets them loaded, then
// the checkpoint restarts once. At most two restarts in a row without
// success, so a failure can never loop.
// Game thread only. Learns the chapter's Ellie outfit from the companion; it
// applies from the next spawn (Outfit=0). No restart: one right after a
// cutscene put the player back as Joel (owner, 2026-11-01).
void CheckChapterOutfit() {
    const uint64_t seen = g_seenCompanionEllie, joel = g_spawnJoelLook, mine = g_playerLook;
    if (!seen || !joel || g_outfit != 0 || g_swappedTo != 1 || g_restarting >= 0 || g_swapWait >= 0) return;
    g_seenCompanionEllie = 0;
    if (seen == mine || ChapterEllie(joel)) return;
    g_chapterOutfit[joel] = seen;
    logx::Write("outfit: this chapter's Ellie wears %s (Joel: %s); the player wore %s; from the next spawn on, she "
                "wears that one", NameOf(seen).c_str(), NameOf(joel).c_str(), NameOf(mine).c_str());
}

void CheckSwappedSpawn() {
    static int seen = 0, wait = 0, streak = 0;
    const int spawns = g_spawns;
    if (spawns == seen || g_swappedTo != 1 || g_swapWait >= 0 || !g_present) {
        if (spawns == seen) wait = 0;
        return;
    }
    if (++wait < 90) return; // ~1.5 s after the spawn
    size_t total = 0;
    int missing = 0;
    const int loaded = LoadedCount(total, &missing);
    if (wait == 90) LogPlayerOutfit(g_playerLook);
    if (total && (missing == 0 || (loaded + 4 >= static_cast<int>(total) && !OutfitPartsMissing()))) {
        seen = spawns;
        streak = 0;
        logx::Write("swap: Ellie spawned with %d of %zu packages loaded", loaded, total);
        if (g_giveRecipes) g_giveAfterSpawn = true;
        StartGunSearch();
        if (g_restarting >= 0) {
            g_restarting = -1;
            Done("Swapped to Ellie");
        } else if (!g_greeted) {
            Say("Playing as Ellie  (F10 to switch)");
        }
        g_greeted = true;
        return;
    }
    if (!g_setsFound) { // the search runs in the background; keep checking for ~1 min
        if (wait == 90) {
            logx::Write("swap: Ellie spawned with %d of %zu packages loaded; waiting for the level sets", loaded, total);
            Busy("Preparing Ellie...");
        }
        if (wait % 600 == 0) PrepareSwap();
        if (wait < 90 + 60 * 60) return;
        seen = spawns;
        logx::Write("swap: level sets not found; Ellie's animations cannot be loaded");
        Done("Ellie's animations could not be loaded (see PlayableEllie.log)");
        Tone(3);
        return;
    }
    seen = spawns;
    logx::Write("swap: Ellie spawned with %d of %zu packages loaded", loaded, total);
    if (streak >= 2) {
        logx::Write("swap: her animations did not load after two restarts; not trying again until a good spawn");
        g_restarting = -1;
        Done("Ellie's animations did not load (see PlayableEllie.log)");
        Tone(3);
        return;
    }
    ++streak;
    g_restarting = -1;
    SyncPatches();
    if (RequestPackages()) {
        g_swapWait = 0;
        Busy("Loading Ellie's animations...", 0.f);
    }
}

// ------------------------------------------------------- overlay research
// Test builds with ResearchOverlays=1 in the ini (this game build only, like
// w1z's menu's research logging): every change of an animation overlay slot
// is logged with its name where known, plus markers for aim (right mouse),
// reload (R) and F9 (press it the moment Ellie freezes or twitches). The
// arms-out freeze looks like the game's "anim not found: bindpose" case, so
// the overlay switched to right before it is the lead. Overlays live in slots
// of an overlay set; every change ends in SetSlot(slots, index, def)
// (0x1FD1C84); 0x1FD1D54 / 0x1FD1D20 apply one by name hash, and 0x1EFBD28
// maps a name hash to its def, which lets the log name them.
#if PE_TEST_BOW
constexpr uint32_t kResearchTimestamp = 0x685BEBE1;
using SetSlotFn = void(__fastcall*)(void*, int, void*);
using ApplyByNameFn = bool(__fastcall*)(void*, uint64_t, void*);
using LookupDefFn = void*(__fastcall*)(uint64_t, void*);
SetSlotFn oSetSlot = nullptr;
ApplyByNameFn oApplyByName = nullptr, oApplyByName2 = nullptr;
LookupDefFn g_lookupDef = nullptr;
std::mutex g_ovlLock;
std::map<uint64_t, std::string> g_ovlNames;       // name hash -> name (from the exe's strings)
std::map<void*, uint64_t> g_ovlDefHash;           // def -> name hash
std::map<std::pair<void*, int>, void*> g_ovlSlot; // (slots, index) -> def
int g_ovlLines = 0, g_ovlSpawn = -1;
bool g_ovlOn = false;

std::string OvlName(void* def) {
    auto it = g_ovlDefHash.find(def);
    if (it == g_ovlDefHash.end()) return "?";
    auto n = g_ovlNames.find(it->second);
    if (n != g_ovlNames.end()) return n->second;
    char b[24];
    std::snprintf(b, sizeof b, "#%016llX", static_cast<unsigned long long>(it->second));
    return b;
}

void __fastcall HkSetSlot(void* slots, int index, void* def) {
    oSetSlot(slots, index, def);
    std::lock_guard<std::mutex> l(g_ovlLock);
    void*& cur = g_ovlSlot[{slots, index}];
    if (g_ovlSpawn != g_spawns) g_ovlSpawn = g_spawns, g_ovlLines = 0;
    if (cur == def || g_ovlLines > 8000) return;
    const std::string from = cur ? OvlName(cur) : "-", to = def ? OvlName(def) : "-";
    cur = def;
    ++g_ovlLines;
    logx::Write("ovl %p[%d] %s -> %s (%s)", slots, index, from.c_str(), to.c_str(), g_swappedTo == 1 ? "Ellie" : "Joel");
}

void RememberDef(uint64_t hash, void* ctx) {
    if (!g_lookupDef) return;
    void* def = g_lookupDef(hash, ctx);
    if (!def) return;
    std::lock_guard<std::mutex> l(g_ovlLock);
    g_ovlDefHash[def] = hash;
}
bool __fastcall HkApplyByName(void* set, uint64_t hash, void* flag) {
    RememberDef(hash, *reinterpret_cast<void**>(static_cast<uint8_t*>(set) + 0x10));
    return oApplyByName(set, hash, flag);
}
bool __fastcall HkApplyByName2(void* set, uint64_t hash, void* ctx) {
    RememberDef(hash, ctx);
    return oApplyByName2(set, hash, ctx);
}

void InstallOverlayResearch() {
    if (!settings::GetBool("ResearchOverlays", false)) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kResearchTimestamp) {
        logx::Write("research: overlay logging needs game build 0x%08X; off", kResearchTimestamp);
        return;
    }
    const size_t size = nt->OptionalHeader.SizeOfImage;
    for (size_t i = 1; i + 8 < size; ++i) { // names: printable strings that look like anim/overlay names
        if (g_base[i - 1] != 0) continue;
        size_t n = 0;
        while (i + n < size && n < 96 && g_base[i + n] >= 0x21 && g_base[i + n] < 0x7F) ++n;
        if (n < 6 || i + n >= size || g_base[i + n] != 0) continue;
        const std::string str(reinterpret_cast<const char*>(g_base + i), n);
        for (const char* w : {"overlay", "aim", "bow", "rifle", "shotgun", "grip", "holster", "ellie", "joel", "long-gun",
                              "reload", "pistol", "gun", "blocked", "idle", "weapon"})
            if (str.find(w) != std::string::npos) {
                g_ovlNames[dscan::Fnv1a64(str.c_str())] = str;
                break;
            }
        i += n;
    }
    g_lookupDef = reinterpret_cast<LookupDefFn>(const_cast<uint8_t*>(g_base + 0x1EFBD28));
    struct H { size_t rva; void* hk; void** orig; } hooks[] = {
        {0x1FD1C84, reinterpret_cast<void*>(&HkSetSlot), reinterpret_cast<void**>(&oSetSlot)},
        {0x1FD1D54, reinterpret_cast<void*>(&HkApplyByName), reinterpret_cast<void**>(&oApplyByName)},
        {0x1FD1D20, reinterpret_cast<void*>(&HkApplyByName2), reinterpret_cast<void**>(&oApplyByName2)},
    };
    int ok = 0;
    for (auto& h : hooks) {
        void* t = const_cast<uint8_t*>(g_base + h.rva);
        if (MH_CreateHook(t, h.hk, h.orig) == MH_OK && MH_EnableHook(t) == MH_OK) ++ok;
    }
    g_ovlOn = ok == 3;
    logx::Write("research: overlay logging %s (%zu names); markers: AIM = right mouse, RELOAD = R, MARK = F9",
                g_ovlOn ? "on" : "FAILED", g_ovlNames.size());
}

// Game thread only. Marker lines in the overlay log.
void ResearchMarkers() {
    if (!g_ovlOn || !GameInForeground()) return;
    static bool aim = false, reload = false, mark = false;
    const bool a = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0, r = (GetAsyncKeyState('R') & 0x8000) != 0,
               m = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    const char* who = g_swappedTo == 1 ? "Ellie" : "Joel";
    if (a != aim) logx::Write("----- AIM %s (%s)", a ? "DOWN" : "UP", who);
    if (r && !reload) logx::Write("----- RELOAD (%s)", who);
    if (m && !mark) {
        logx::Write("===== MARK: freeze or twitch now (%s)", who);
        Say("Marked in the log");
    }
    aim = a, reload = r, mark = m;
}
#else
void InstallOverlayResearch() {}
void ResearchMarkers() {}
#endif

void Tick() {
    ResearchMarkers();
    CheckChapterOutfit();
    g_present = gameloop::PlayerPresent(); // once per frame; everything below uses it
    if (g_toggleKey) {
        static bool down = false;
        const bool now = (GetAsyncKeyState(g_toggleKey) & 0x8000) != 0 && GameInForeground();
        if (now && !down) OnToggleKey();
        down = now;
    }
    if (PE_TEST_BOW) { // test builds: F11 cycles the bow settings while playing
        static bool down = false;
        const bool now = (GetAsyncKeyState(VK_F11) & 0x8000) != 0 && GameInForeground();
        if (now && !down) {
            g_bowMode = (g_bowMode + 1) % kBowModes;
            char msg[128];
            std::snprintf(msg, sizeof msg, "Bow %d: %s (switch weapons to see it)", g_bowMode, kBowModeNames[g_bowMode]);
            Say(msg);
            SyncBowHolster(g_on || g_swappedTo == 1);
        }
        down = now;
    }
    if (PE_TEST_9MM) { // test builds: F11 cycles the 9mm tables while playing
        static bool down = false;
        const bool now = (GetAsyncKeyState(VK_F11) & 0x8000) != 0 && GameInForeground();
        if (now && !down) {
            g_gunMode = (g_gunMode + 1) % kGunModes;
            char msg[128];
            std::snprintf(msg, sizeof msg, "9mm table %d: %s", g_gunMode, kGunModeNames[g_gunMode]);
            Say(msg);
            if (!g_9mmOverlayCount) StartGunSearch();
            SyncGunOverlay(g_on || g_swappedTo == 1);
        }
        down = now;
    }
    CheckSwappedSpawn();

    // No player for ~2 s (main menu, chapter load): the next chapter may be
    // Joel's, so its companion Ellie becomes Joel again even before Ellie spawns.
    static int absent = 0;
    if (g_present) absent = 0;
    else if (++absent == 120 && g_nativeEllie.exchange(false)) logx::Write("ellie: no player; Ellie's-own-chapter mode cleared");

    // A restart the mod started: Ellie's spawn is announced by
    // CheckSwappedSpawn once her animations are confirmed; Joel's here.
    if (g_restarting >= 0) {
        ++g_restarting;
        if (g_spawns != g_restartSpawns && g_swappedTo != 1 && g_present && g_restarting >= 30) {
            g_restarting = -1;
            Done("Swapped to Joel");
        } else if (g_restarting >= 30 * 60) { // no spawn after ~30 s: stop waiting
            g_restarting = -1;
            overlay::ClearStatus();
            logx::Write("swap: no player spawn within 30 s of the restart");
        }
    }

    // Ellie on at launch: find the level sets once the game is up. Not before
    // ~5 s of game frames (the first search at startup always failed: the
    // records are not in memory yet), then every ~10 s for a while.
    static int frames = 0;
    ++frames;
    if (g_on && !g_setsFound && !g_setsSearching && frames >= 300 && (frames - 300) % 600 == 0 && frames <= 6300)
        PrepareSwap();

    // A toggle that waits for the level-set search.
    if (g_pendingSwap >= 0) {
        if (g_setsFound) {
            g_pendingSwap = -1;
            SwapNow();
        } else if (++g_pendingSwap >= 30 * 60) {
            g_pendingSwap = -1;
            logx::Write("swap: level sets not found; cancelled");
            Done("Ellie could not be prepared (see PlayableEllie.log)");
            Tone(3);
        }
    }

    // The swap waits here until Ellie's packages are in, then restarts. The
    // one package that never loads in Joel's chapters is not waited for; as a
    // fallback it also goes ahead once the count has stopped rising for ~5 s
    // with nearly all in.
    static int lastLoaded = -1, stableSince = 0;
    if (g_swapWait == 0) lastLoaded = -1;
    if (g_swapWait >= 0 && g_present && ++g_swapWait % 10 == 0) {
        size_t total = 0;
        int missing = 0;
        const int loaded = LoadedCount(total, &missing);
        if (loaded != lastLoaded) lastLoaded = loaded, stableSince = g_swapWait;
        const bool settled = g_swapWait - stableSince >= 300 && loaded + 4 >= static_cast<int>(total);
        if (missing == 0 || settled) {
            logx::Write("swap: %d of %zu Ellie packages loaded after %d frames", loaded, total, g_swapWait);
            LogMissing();
            g_swapWait = -1;
            Tone(2);
            DoRestart();
        } else if (g_swapWait >= 45 * 60) {
            LogMissing();
            logx::Write("swap: gave up waiting, %d of %zu Ellie packages loaded; no restart", loaded, total);
            g_swapWait = -1;
            Done("Ellie's animations did not load, so the swap was cancelled");
            Tone(3);
        } else {
            char msg[64];
            std::snprintf(msg, sizeof msg, "Loading Ellie's animations...  %d / %zu", loaded, total);
            Busy(msg, total ? static_cast<float>(loaded) / static_cast<float>(total) : 0.f);
            if (g_swapWait % 120 == 0) logx::Write("swap: waiting, %d of %zu Ellie packages loaded", loaded, total);
        }
    }

    // Removes the patch once Ellie is no longer the one playing.
    static int sync = 0;
    if (++sync % 60 == 0) SyncPatches();

    // After a restart as Ellie, log every ~2 s for ~10 s how many packages are loaded.
    if (g_reportFrames >= 0 && g_present && ++g_reportFrames % 120 == 0) {
        size_t total = 0;
        const int loaded = LoadedCount(total);
        logx::Write("swap: %d of %zu Ellie packages loaded", loaded, total);
        if (loaded + 1 >= static_cast<int>(total) || g_reportFrames >= 600) g_reportFrames = -1;
    }

    // The recipes are given ~3 s after she is up and running with her
    // animations (giving during the restart itself raised an exception in
    // w1z's menu).
    static int wait = 0;
    if (!g_giveAfterSpawn || g_swapWait >= 0 || !g_present) {
        wait = 0;
        return;
    }
    if (++wait < 180) return;
    g_giveAfterSpawn = false;
    if (g_swappedTo == 1) GiveRecipes();
}

} // namespace

void Init(const uint8_t* base, const resolve::Addresses& a) {
    g_base = base;
    auto at = [base](size_t rva) { return const_cast<uint8_t*>(base + rva); };
    g_restart = reinterpret_cast<Restart>(at(a.restart));
    g_loaded = reinterpret_cast<PackageState>(at(a.packageLoaded));
    g_packageMgr = reinterpret_cast<void**>(at(a.packageMgr));
    g_levelMgr = reinterpret_cast<void**>(at(a.levelSets.mgrGlobal));
    g_rebuildWants = reinterpret_cast<RebuildWants>(at(a.levelSets.rebuildWants));

    g_on = settings::GetBool("PlayAsEllie", true);
    g_toggleKey = settings::GetInt("ToggleKey", VK_F10);
    if (g_toggleKey < 0 || g_toggleKey > 0xFE) g_toggleKey = 0;
    g_outfit = settings::GetInt("Outfit", 0);
    g_giveRecipes = settings::GetBool("GiveRecipes", true);
    g_gunMode = settings::GetInt("Fix9mm", 1);
    if (g_gunMode < 0 || g_gunMode >= (PE_TEST_9MM ? kGunModes : 2)) g_gunMode = 1;
    g_notify = settings::GetBool("Notifications", true);
    g_sounds = settings::GetBool("Sounds", false);
    g_bowMode = settings::GetBool("BowOnBack", true) ? 2 : 0;
    const bool wantBuddy = settings::GetBool("JoelCompanion", true);
    logx::Write("settings: PlayAsEllie=%d ToggleKey=%d JoelCompanion=%d Outfit=%d GiveRecipes=%d Fix9mm=%d Notifications=%d "
                "Sounds=%d BowOnBack=%d",
                g_on ? 1 : 0, g_toggleKey, wantBuddy ? 1 : 0, g_outfit, g_giveRecipes ? 1 : 0, g_gunMode,
                g_notify ? 1 : 0, g_sounds ? 1 : 0, g_bowMode ? 1 : 0);

    BuildMaps();
    if (!LoadPackageLists()) {
        logx::Write("ellie: OFF - Ellie's package list could not be read, so her animations cannot be checked");
        Say("Playable Ellie is off: the game's package list could not be read");
        return;
    }
    g_patches = {{"joel-player-all", {"ellie-player-all", "ellie-player-anims-traversal-all"}, {"t1x-anim-ellie-pistol"}},
                 {"joel-player-all-no-explosives", {"ellie-player-all-no-explosives", "ellie-player-anims-traversal-all"},
                  {"t1x-anim-ellie-pistol"}}};

    // Optional parts.
    if (a.ellieCraftFlag) {
        g_ellieCraftFlag = at(a.ellieCraftFlag);
        logx::Write("ellie: crafting flag RVA 0x%zX", a.ellieCraftFlag);
    } else {
        logx::Write("ellie: crafting outside Winter OFF (flag not found)");
    }
    if (a.allRecipes) g_allRecipes = at(a.allRecipes);
    else logx::Write("ellie: Give Recipes OFF (All Recipes not found)");
    if (a.skinLookReturn) g_skinLookReturn = base + a.skinLookReturn;
    logx::Write("ellie: Skins menu look call %s", g_skinLookReturn ? "found (left alone)" : "NOT found");
    if (a.levelLookReturn) g_levelLookReturn = base + a.levelLookReturn;
    logx::Write("ellie: level-load look call %s", g_levelLookReturn ? "found (left alone)" : "NOT found");
    if (wantBuddy && a.lookResolve) {
        void* lookFn = at(a.lookResolve);
        if (MH_CreateHook(lookFn, reinterpret_cast<void*>(&HkResolveLook), reinterpret_cast<void**>(&oResolveLook)) == MH_OK &&
            MH_EnableHook(lookFn) == MH_OK) {
            g_joelBuddy = true;
            logx::Write("ellie: look resolver RVA 0x%zX hooked (Joel companion on)", a.lookResolve);
        } else {
            logx::Write("ellie: Joel companion OFF (hooking the look resolver failed)");
        }
    } else {
        logx::Write("ellie: Joel companion OFF (%s)", wantBuddy ? "look resolver not found" : "JoelCompanion=0");
    }

    void* core = at(a.coreSpawn);
    if (MH_CreateHook(core, reinterpret_cast<void*>(&HkCore), reinterpret_cast<void**>(&oCore)) != MH_OK ||
        MH_EnableHook(core) != MH_OK) {
        logx::Write("ellie: OFF - hooking the player spawn failed");
        if (g_joelBuddy) MH_DisableHook(at(a.lookResolve));
        g_joelBuddy = false;
        Say("Playable Ellie is off: the player spawn could not be hooked");
        return;
    }
    logx::Write("ellie: core spawn RVA 0x%zX hooked; restart checkpoint RVA 0x%zX", a.coreSpawn, a.restart);
    InstallOverlayResearch();
    gameloop::SetTick(&Tick); // the level-set search starts from there once the game runs
    logx::Write("ellie: ready. Play as Ellie is %s; press %s to change it in game", g_on ? "ON" : "OFF",
                g_toggleKey == VK_F10 ? "F10" : g_toggleKey ? "the ToggleKey" : "(no key set)");
}

} // namespace ellie
