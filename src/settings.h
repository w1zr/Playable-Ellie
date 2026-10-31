// Settings in Documents\Playable Ellie (WORKING)\PlayableEllie.ini, outside
// the game folder, so updating or reinstalling the mod (or Fluffy) never
// resets them. Reads and writes go straight to the file
// (Get/WritePrivateProfileString). Section [PlayableEllie].
#pragma once
#include <string>

namespace settings {

// Creates the file with the defaults (and its folder) if it does not exist.
void Init(const wchar_t* iniPath);
const std::wstring& Path();
bool CreatedThisRun();
// True when Init rewrote an older file with the new keys (values kept).
bool UpgradedThisRun();

bool GetBool(const char* key, bool def);
int GetInt(const char* key, int def);
void SetBool(const char* key, bool value);

} // namespace settings
