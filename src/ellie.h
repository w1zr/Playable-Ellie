// Play Joel's chapters as Ellie: the player-spawn swap with outfit matching,
// her animations streamed in before a checkpoint restart, crafting in every
// chapter with all recipes, Joel's 9mm held straight, and Joel as the AI
// companion. Made from w1z's menu 1.2.0's src/character.cpp (tested in game
// there on 2026-10-30).
#pragma once
#include "resolve.h"

namespace ellie {
// Installs the hooks and starts the per-frame work. Startup thread, after
// gameloop::Install succeeded and with MinHook initialised.
void Init(const uint8_t* base, const resolve::Addresses& a);
} // namespace ellie
