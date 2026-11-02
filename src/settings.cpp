#include "settings.h"

#include <windows.h>
#include <cstdio>
#include <mutex>

namespace settings {

namespace {

std::wstring g_ini;
std::mutex g_lock;
bool g_created = false, g_upgraded = false;

constexpr wchar_t kSection[] = L"PlayableEllie";

std::wstring Wide(const char* s) {
    std::wstring w;
    while (*s) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*s++)));
    return w;
}

// File contents with every key's value filled in. Comments explain every
// key; kDefaults are the shipped values. Kept in sync with dist/PlayableEllie.ini.
const char kTemplate[] =
    "; Playable Ellie (WORKING) - settings. The mod saves the on/off key's state here.\r\n"
    "\r\n"
    "[PlayableEllie]\r\n"
    "; 1 = play Joel's chapters as Ellie. The toggle key changes it in game.\r\n"
    "PlayAsEllie=%d\r\n"
    "; Key that turns Ellie on / off in game and restarts from your last checkpoint\r\n"
    "; as the other character (Windows virtual-key code, 0 = no key). 121 = F10.\r\n"
    "ToggleKey=%d\r\n"
    "; 1 = the story companion Ellie becomes Joel while you play as Ellie.\r\n"
    "JoelCompanion=%d\r\n"
    "; Ellie's outfit: 0 = the one Ellie wears in the chapter, 1-11 = a fixed outfit\r\n"
    "; (costume 01, 02, 03, 04, 05, 06, 07, 08, 11, 13, 15). An Ellie skin chosen\r\n"
    "; in Extras > Skins is drawn over it; pick Default there to see this one.\r\n"
    "Outfit=%d\r\n"
    "; 1 = teach Ellie every crafting recipe after she spawns, so the crafting\r\n"
    "; menu has something to show (the game saves it at the next checkpoint).\r\n"
    "GiveRecipes=%d\r\n"
    "; 1 = Joel's 9mm sits straight in Ellie's hands. Known issue: when it runs\r\n"
    "; dry and the game switches weapons by itself, her arms stay out until you\r\n"
    "; aim. 0 = no freeze, but the 9mm tilts up in her hands.\r\n"
    "Fix9mm=%d\r\n"
    "; 1 = small on-screen messages: loading Ellie's animations, swapped, errors.\r\n"
    "; Off by itself when ReShade or OptiScaler is installed (both together crash).\r\n"
    "Notifications=%d\r\n"
    "; 1 = also beep when the toggle key is pressed and when the swap is done.\r\n"
    "Sounds=%d\r\n"
    "; 1 = write PlayableEllie.log next to the game exe.\r\n"
    "Logging=%d\r\n"
    "; 1 = every Extras > Skins outfit and weapon skin unlocked and owned (no points\r\n"
    "; spent, saves untouched), like TLOU Outfit Changer. 0 = the game's own locks.\r\n"
    "UnlockSkins=%d\r\n"
    "; 1 = Joel's bow uses Ellie's bow settings while you play as Ellie, so it sits\r\n"
    "; vertically on her back instead of across her backpack.\r\n"
    "BowOnBack=%d\r\n"
    "; Changes in this file apply the next time the game starts.\r\n";

struct Key {
    const wchar_t* name;
    int def;
};
const Key kKeys[] = {{L"PlayAsEllie", 1}, {L"ToggleKey", 121},    {L"JoelCompanion", 1}, {L"Outfit", 0}, {L"GiveRecipes", 1},
                     {L"Fix9mm", 1},      {L"Notifications", 1}, {L"Sounds", 0},        {L"Logging", 1},
                     {L"UnlockSkins", 1}, {L"BowOnBack", 1}};
constexpr int kKeyCount = static_cast<int>(sizeof(kKeys) / sizeof(kKeys[0]));

bool Write(const wchar_t* path, const int (&v)[kKeyCount], DWORD how) {
    char text[sizeof(kTemplate) + 200];
    const int n = std::snprintf(text, sizeof text, kTemplate, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
    if (n <= 0 || n >= static_cast<int>(sizeof text)) return false;
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, how, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(f, text, static_cast<DWORD>(n), &written, nullptr) && written == static_cast<DWORD>(n);
    CloseHandle(f);
    return ok;
}

} // namespace

void Init(const wchar_t* iniPath) {
    g_ini = iniPath;
    int v[kKeyCount];
    if (GetFileAttributesW(iniPath) == INVALID_FILE_ATTRIBUTES) {
        std::wstring dir(iniPath);
        dir = dir.substr(0, dir.find_last_of(L'\\'));
        CreateDirectoryW(dir.c_str(), nullptr);
        for (int i = 0; i < kKeyCount; ++i) v[i] = kKeys[i].def;
        g_created = Write(iniPath, v, CREATE_NEW);
        return;
    }
    // A file from an older version (a key missing): rewritten with every key
    // and comment, keeping every value the player had.
    bool complete = true;
    for (const Key& k : kKeys) complete = complete && GetPrivateProfileIntW(kSection, k.name, -12345, iniPath) != static_cast<UINT>(-12345);
    if (complete) return;
    for (int i = 0; i < kKeyCount; ++i)
        v[i] = static_cast<int>(GetPrivateProfileIntW(kSection, kKeys[i].name, kKeys[i].def, iniPath));
    g_upgraded = Write(iniPath, v, CREATE_ALWAYS);
}

const std::wstring& Path() { return g_ini; }
bool CreatedThisRun() { return g_created; }
bool UpgradedThisRun() { return g_upgraded; }

bool GetBool(const char* key, bool def) { return GetInt(key, def ? 1 : 0) != 0; }

int GetInt(const char* key, int def) {
    std::lock_guard<std::mutex> l(g_lock);
    return static_cast<int>(GetPrivateProfileIntW(kSection, Wide(key).c_str(), def, g_ini.c_str()));
}

void SetBool(const char* key, bool value) {
    std::lock_guard<std::mutex> l(g_lock);
    WritePrivateProfileStringW(kSection, Wide(key).c_str(), value ? L"1" : L"0", g_ini.c_str());
}

} // namespace settings
