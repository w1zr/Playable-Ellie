// Signatures and data for Playable Ellie, taken unchanged from w1z's menu
// 1.2.0 (src/engine_defs.h), where they were tested in game. Shared by the
// mod (resolve.cpp) and the offline test (tests/scan_test.cpp), so the test
// checks exactly what the mod will hook.
#pragma once
#include <cstddef>

namespace engine {

// Developer-menu items the mod uses, by exact label.
inline const char* kAllRecipesLabel = "All Recipes";      // ItemBool with callback; data = hash("all")
inline const char* kAllRecipesData = "all";
inline const char* kBoolCallbackAnchor = "Drop All Weapons"; // a known ItemBool-with-callback, for its constructor
inline const char* kFunctionAnchor = "Give Each Craftable";  // a known ItemFunction; its start finds the player lookup

// Prologue of GameLoopUpdate(unsigned __int64), the per-frame game loop the
// mod runs its game-thread work from. Found through its profiling name string.
inline const char* kGameLoopName = "void __cdecl GameLoopUpdate(unsigned __int64)";
inline const char* kGameLoopPrologue =
    "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 81 EC ?? ?? ?? ?? 48 8D A8";

// Character swap. The developer menu's "Spawn Player" item toggles the player:
// it despawns an existing player, or spawns one at the camera through the
// player-spawn function, passing a character id (0 = the default, t1x-joel).
// That function calls the core spawn every player spawn goes through
// (checkpoints, deaths, chapter loads) with the character id as its fifth
// argument; the mod swaps that id.
inline const char* kSpawnPlayerLabel = "Spawn Player";
// In the callback: mov r8,[rip+id]; lea rcx,[rsp+20h]; xor r9d,r9d; call spawn
inline const char* kSpawnCall = "4C 8B 05 ?? ?? ?? ?? 48 8D 4C 24 20 45 33 C9 E8";
// In the spawn function: movabs r14, <default character id>
inline const char* kDefaultCharacter = "49 BE";
// ... and: mov [rsp+40h], rdi; call core
inline const char* kCoreCall = "48 89 7C 24 40 E8";
inline const char* kCorePrologue = "40 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 ?? ?? ?? ?? B8 ?? ?? ?? ?? E8";
inline const char* kJoel = "t1x-joel";
inline const char* kEllie = "t1x-ellie";

// Start of the Give Each Craftable callback: sub rsp,28h; lea rcx,[rip+player
// handle]; mov r9d,edx; call player lookup.
inline const char* kPlayerLookupStart = "48 83 EC 28 48 8D 0D ?? ?? ?? ?? 44 8B CA E8";

// GameRestartCheckpoint(bool, bool): the pause menu's Restart Checkpoint calls
// it with (false, false). Found through its profiling name string.
inline const char* kRestartName = "bool __cdecl GameRestartCheckpoint(bool,bool)";
inline const char* kRestartPrologue = "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 4C 89 60 20 55 41 56 41 57 48 8D 68 A1";

// Player animation packages. A chapter loads only its own player's level set
// (joel-player-all / ellie-player-all in level-sets-common), so a swapped
// player is missing its standing movement, aiming, throwing and so on. These
// are the actor packages in Ellie's set (plus traversal and water) that
// Joel's set does not load, from the game's level-sets
// data (anim-ellie-player-water-u4 is listed there but is not a PC package).
// The mod counts them with the engine's package checks, and only the
// ones in the game's build\pc\main\actor97.txt: asking for a package the
// game has no definition for trips an assert in LevelDefCollection.
//
// At a call site in the game: mov rcx,[rip+package manager]; call
// IsLoadedOrLoading(mgr, FNV-1a name hash); test al,al; jnz; mov r8b,1;
// xor edx,edx; mov rcx,rdi; call LoadPackage(name, 0, flag).
inline const char* kPackageCheckCall = "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 84 C0 75 0D 41 B0 01 33 D2 48 8B CF E8";
inline const char* kLoadPackagePrologue = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 50 41 BF 02 00 00 00";
// IsLoadedOrLoading: state 11 or 12. IsLoaded (state 12) follows 0x44 later.
inline const char* kPackageLoadingCheck = "48 83 EC 28 E8 ?? ?? ?? ?? 48 85 C0 74 0F 8B 40 40 B9 01 00 00 00 83 C0 F5";
inline const char* kPackageLoadedCheck = "48 83 EC 28 E8 ?? ?? ?? ?? 48 8B C8 33 C0 48 85 C9 74 08 83 79 40 0C";
inline constexpr size_t kPackageLoadedOffset = 0x44;

// The developer console's LoadLevelSet command is registered as
//     lea r9, [args]; lea rdx, ["LoadLevelSet"]; lea rcx, [handler]; jmp register
// and its handler calls LevelMgr::SetLevelSet(g_levelMgr, hash, true, true).
// SetLevelSet rebuilds the want-load list from the current level set's tree
// with the function behind kRebuildWantsCall. Calling that rebuild ourselves
// makes the game stream in the sets a patched record adds, during normal play.
inline const char* kLevelSetCommand = "LoadLevelSet";
inline const char* kConsoleRegister = "4C 8D 0D ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E9";
inline const char* kSetLevelSetCall = "41 B1 01 48 8B 0D ?? ?? ?? ?? 45 8A C1 E8";
inline const char* kSetLevelSetPrologue =
    "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 90";
inline const char* kRebuildWantsCall = "44 38 74 24 32 74 08 49 8B CD E8";
// Crafting for Ellie. The crafting controller's "crafting allowed" check lets
// an Ellie player craft only while the Winter world "lakeside" is loaded, or
// when a game flag (off by default, read only here) is set; that is why every
// Ellie swap could only craft in Winter. kEllieCraftFlagRead is the flag read
// right after the lakeside test (the "lakeside" string must be referenced in
// the 0x80 bytes before it).
inline const char* kEllieCraftLevel = "lakeside";
inline const char* kEllieCraftFlagRead = "40 84 F6 75 09 40 38 35 ?? ?? ?? ?? 74 03 40 8A F7 40 84 ED";
inline const char* kRebuildWantsPrologue = "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 48 8D A8 E8 35 FF FF B8 10 CB 00 00";

// Joel as the AI companion. Every character's look (e.g. t1x-ellie-costume-02)
// is resolved by name hash in this function (rdx), after the game's own look
// remap list and before its look2 data is read. The companion's look comes
// from its spawner, so changing the hash here gives the same buddy (its AI,
// scripts and dialogue) Joel's body.
// SkinManager::Skin (the Extras > Skins menu and its previews) asks the look
// resolver for each skin's look here; the companion swap must leave those alone
// (an Ellie skin preview turned into Joel crashed the game, a player on Nexus,
// 2026-11-01). The call at +0x3E goes to the look resolver; its return address
// is +0x43.
inline const char* kSkinLookCall =
    "48 89 5C 24 18 55 56 57 41 56 41 57 48 8B EC 48 81 EC 80 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 F0 "
    "C5 F9 EF C0 48 8B F2 4C 8B F1 48 8B 51 08 48 8D 4D D0 C5 FC 11 45 D0 C5 F8 77 E8 ?? ?? ?? ?? "
    "48 BF 25 23 22 84 E4 9C F2 CB";
constexpr size_t kSkinLookCallAt = 0x3E, kSkinLookReturn = 0x43;
// While a level loads, the game registers the looks it may need (a loop over
// the level's look list, each resolved through the look resolver). Turned into
// Joel there, the Skins menu's Ellie previews showed Joel for the whole session
// (a player on Nexus, 2026-11-01), so these are left alone; characters still
// get Joel when they spawn. The call is at +0x2E; its return address is +0x33.
inline const char* kLevelLookCall =
    "48 BB 25 23 22 84 E4 9C F2 CB 4D 8B E5 4C 8B 2E 4D 39 75 08 0F 84 ?? ?? ?? ?? 49 8B 55 08 48 8D 4D C0 "
    "C5 F9 EF C0 C5 FC 11 45 C0 C5 F8 77 E8 ?? ?? ?? ?? 48 8B D3 48 8D 0D";
constexpr size_t kLevelLookCallAt = 0x2E, kLevelLookReturn = 0x33;
inline const char* kLookResolve =
    "48 89 5C 24 10 48 89 6C 24 18 56 57 41 54 41 56 41 57 48 83 EC 70 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 60 "
    "48 8B F9 48 8D 4C 24 20 E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 4C 8D 4C 24 50 45 33 FF 48 8D 54 24 48 48 8B 18 4C "
    "89 7C 24 50 4C 8B C3 4C 89 7C 24 58 48 8B 01 FF 90 80 02 00 00";
// What the game loads for Joel as a companion in the epilogue, where Ellie is
// the player and Joel follows her (level-sets-lab: tsk-lab-3-post-fence). His
// body, facial and gesture animations are already in Joel's chapters.
inline const char* const kJoelBuddySets[] = {"npc-buddy-all"};
inline const char* const kJoelBuddyPackages[] = {"t1x-anim-npc-joel", "anim-npc-m-buddy-ambi"};

// 49 packages
inline const char* const kEllieOnlyPackages[] = {
    "t1x-rifle-bolt-m24", "t1x-bow-ellie", "pistol-revolver-hunter", "rifle-bolt-hunter", "shotgun-pump-hunter",
    "anim-ellie-player-mm-explore", "anim-ellie-player-mm-uneasy", "anim-shared-player-mm-explore-scripted-goto",
    "anim-shared-player-mm-crouch-scripted-goto", "anim-t1x-ellie-inspect", "anim-t1x-ellie-inspect-backpack",
    "t1x-anim-ellie-player-scavenge", "t1x-anim-ellie-player", "joel-skel", "t1x-joel-skel", "base-female-skel",
    "prop-backpack-ellie-skel", "prop-backpack-ellie", "prop-backpack-ellie-02", "prop-backpack-ellie-03",
    "craft-backpack-ellie-skel", "craft-backpack-ellie", "craft-backpack-ellie-02", "craft-backpack-ellie-03",
    "anim-ellie-player-hands", "anim-ellie-t1x-facial", "t1x-anim-ellie-player-gestures", "anim-exhausted-breathing",
    "anim-shared-player-healing", "anim-ellie-player-throw", "t1x-anim-ellie-player-mm-aim", "t1x-flashlight-ellie",
    "t1x-anim-ellie-player-mm-cover", "anim-ellie-player-mm-tense", "anim-ellie-player-mm-tense-shared",
    "anim-ellie-player-mm-melee", "t1x-anim-ellie-melee-common", "anim-melee-ellie-t2", "t1x-anim-ellie-v-thug-melee",
    "t1x-anim-ellie-v-inf-melee", "t1x-anim-ellie-v-inf-dlc-melee", "anim-ellie-player-balance",
    "anim-ellie-lose-balance", "anim-ellie-player-ledge-walk", "t1x-anim-ellie-player-mm-shimmy",
    "anim-ellie-player-ladder", "anim-ellie-player-ladder-carry", "anim-ellie-player-mm-wade",
    "t1x-anim-ellie-pistol", // her animations for Joel's 9mm (see ellie.cpp)
};
// Outfit characters in the game's character data (spawn-character.bin).
// Joel's chapter outfit N becomes Ellie's outfit N (or her closest earlier one).
inline const int kEllieOutfits[] = {1, 2, 3, 4, 5, 6, 7, 8, 11, 13, 15};
inline const int kEllieNoBackpack[] = {1, 2, 3, 8};
inline const int kJoelOutfits[] = {1, 2, 3, 4, 5, 7, 14, 15, 16};
inline const int kJoelNoBackpack[] = {2, 4, 14, 15, 16};

} // namespace engine
