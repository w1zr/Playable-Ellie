// On-screen messages in the style of w1z's menu: a small dark box at the
// bottom left with an orange bar. A status line (with a spinner, and an
// optional progress bar) stays while work is going on; toasts fade out after
// a few seconds. Drawn with Dear ImGui right before each DirectX 12 Present
// (the menu's render.cpp without the menu and input). Nothing is drawn, and
// ImGui does no work, while there is no message.
#pragma once
#include <string>

namespace overlay {
// Installs the DX12 hooks (MinHook must be initialised). Early in startup,
// before the game creates its swapchain. False (and logged) on failure; the
// mod then works without messages.
bool Install();
// A message shown for ~3 s. Any thread.
void Toast(const std::string& text);
// A message with a spinner that stays until ClearStatus or the next Status.
// progress: 0..1 draws a bar under it, < 0 none. Any thread.
void Status(const std::string& text, float progress = -1.f);
void ClearStatus();
} // namespace overlay
