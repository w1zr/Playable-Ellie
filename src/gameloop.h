// The game thread: a hook on GameLoopUpdate runs the mod's per-frame work and
// queued jobs at the start of every frame, where calling game functions is
// safe. (The game-loop part of w1z's menu's engine.cpp.)
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace gameloop {

// Hooks GameLoopUpdate at `loop` (MinHook must be initialised). Also takes the
// developer menu's player handle and lookup, for PlayerPresent.
bool Install(const uint8_t* base, size_t loop, size_t playerHandle, size_t playerLookup);

// Called every frame on the game thread (before queued work).
void SetTick(void (*tick)());
// Queues work for the game thread (runs at the start of the next frame).
void RunOnGameThread(std::function<void()> fn);
// True when a player exists (in gameplay, not on the main menu). Game thread.
bool PlayerPresent();
// Runs a developer-menu callback(item, 5) with the item data at +0x60 set to
// `data`. Game thread only. False if the callback raised an exception.
bool RunDevCallback(const uint8_t* callback, uint64_t data);

} // namespace gameloop
