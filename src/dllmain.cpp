// Playable Ellie (WORKING): play Joel's chapters of The Last of Us Part I as
// Ellie. Loads as hid.dll next to tlou-i.exe (see proxy_hid.cpp).
#include "ellie.h"
#include "gameloop.h"
#include "log.h"
#include "overlay.h"
#include "proxy_hid.h"
#include "resolve.h"
#include "settings.h"
#include "version.h"

#include "../third_party/minhook/include/MinHook.h"

#include <windows.h>
#include <psapi.h>
#include <shlobj.h>
#include <cstring>
#include <wchar.h>

namespace {

bool IsSupportedHost() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
    const wchar_t* name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    return _wcsicmp(name, L"tlou-i.exe") == 0 || _wcsicmp(name, L"tlou-i-l.exe") == 0;
}

// Builds "<game folder>\<file>" from the host executable's path.
bool GamePath(const wchar_t* file, wchar_t* out, DWORD cap) {
    const DWORD n = GetModuleFileNameW(nullptr, out, cap);
    if (n == 0 || n >= cap) return false;
    wchar_t* slash = wcsrchr(out, L'\\');
    if (!slash) return false;
    slash[1] = 0;
    return wcscat_s(out, cap, file) == 0;
}

bool Exists(const wchar_t* file) {
    wchar_t p[MAX_PATH];
    return GamePath(file, p, MAX_PATH) && GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

// True when a DLL with this file name, loaded from the game folder, contains
// `tag` in its read-only data (used to recognise the other TLOU mods).
bool GameFolderModLoaded(const wchar_t* dllName, const char* tag) {
    wchar_t dir[MAX_PATH];
    if (!GamePath(L"", dir, MAX_PATH)) return false;
    HMODULE mods[1024];
    DWORD bytes = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &bytes)) return false;
    const size_t tagLen = std::strlen(tag);
    for (DWORD i = 0; i < bytes / sizeof(HMODULE) && i < 1024; ++i) {
        wchar_t path[MAX_PATH];
        if (!GetModuleFileNameW(mods[i], path, MAX_PATH)) continue;
        const wchar_t* name = wcsrchr(path, L'\\');
        if (!name || _wcsicmp(name + 1, dllName) != 0 || _wcsnicmp(path, dir, wcslen(dir)) != 0) continue;
        if (!tag[0]) return true;
        const auto* b = reinterpret_cast<const uint8_t*>(mods[i]);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(b + reinterpret_cast<const IMAGE_DOS_HEADER*>(b)->e_lfanew);
        const auto* sec = IMAGE_FIRST_SECTION(nt);
        for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
            if (!(sec[s].Characteristics & IMAGE_SCN_MEM_READ) || (sec[s].Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
            const size_t lo = sec[s].VirtualAddress, hi = lo + sec[s].Misc.VirtualSize;
            for (size_t k = lo; k + tagLen <= hi; ++k)
                if (std::memcmp(b + k, tag, tagLen) == 0) return true;
        }
    }
    return false;
}

bool ImageInfo(const uint8_t*& base, size_t& textEnd, size_t& imageSize, uint32_t& stamp) {
    base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    imageSize = nt->OptionalHeader.SizeOfImage;
    stamp = nt->FileHeader.TimeDateStamp;
    const auto* sec = IMAGE_FIRST_SECTION(nt);
    textEnd = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            textEnd = sec[i].VirtualAddress + sec[i].Misc.VirtualSize;
            break;
        }
    }
    return textEnd != 0;
}

DWORD WINAPI Startup(LPVOID) {
    // w1z's menu (winmm.dll) already has all of this on its Character page;
    // two copies would hook the same functions. Its static imports are loaded
    // by now (this thread starts after the loader lock is released).
    if (GameFolderModLoaded(L"winmm.dll", "w1z's menu") || (Exists(L"winmm.dll") && Exists(L"w1zsMenu.log"))) {
        logx::Write("w1z's menu is installed (winmm.dll); it already has Play as Ellie on its Character page, so "
                    "Playable Ellie steps aside and changes nothing");
        return 0;
    }
    if (GameFolderModLoaded(L"avrt.dll", ""))
        logx::Write("note: avrt.dll in the game folder (TLOU Co-op?). Playable Ellie is single-player only and was "
                    "not tested together with it");

    if (MH_Initialize() != MH_OK) {
        logx::Write("ERROR: MinHook could not initialise; the mod is off");
        return 0;
    }
    // The message overlay goes in first: its hooks must be in before the game
    // creates its swapchain.
    const bool messages = settings::GetBool("Notifications", true) && overlay::Install();

    const uint8_t* base = nullptr;
    size_t textEnd = 0, size = 0;
    uint32_t stamp = 0;
    if (!ImageInfo(base, textEnd, size, stamp)) {
        logx::Write("ERROR: cannot read the game image; the mod is off");
        return 0;
    }
    logx::Write("game image timestamp 0x%08X, size 0x%zX", stamp, size);
    const ULONGLONG t0 = GetTickCount64();
    const resolve::Result r = resolve::Find(base, textEnd, size);
    const resolve::Addresses& a = r.a;
    logx::Write("scan took %llu ms: game loop 0x%zX, core spawn 0x%zX, restart 0x%zX, player lookup 0x%zX, level manager "
                "0x%zX, want-load rebuild 0x%zX, package manager 0x%zX, IsLoaded 0x%zX, look resolver 0x%zX, crafting "
                "flag 0x%zX, All Recipes 0x%zX",
                GetTickCount64() - t0, a.gameLoop, a.coreSpawn, a.restart, a.playerLookup, a.levelSets.mgrGlobal,
                a.levelSets.rebuildWants, a.packageMgr, a.packageLoaded, a.lookResolve, a.ellieCraftFlag, a.allRecipes);
    for (const auto& e : r.errors) logx::Write("not found: %s", e.c_str());
    if (!r.RequiredOk()) {
        logx::Write("the mod is OFF: a part Ellie needs was not found in this game version, so nothing was changed");
        if (messages) overlay::Toast("Playable Ellie is off: this game version is not supported (see PlayableEllie.log)");
        return 0;
    }
    if (!gameloop::Install(base, a.gameLoop, a.playerHandle, a.playerLookup)) {
        logx::Write("the mod is OFF: the game loop could not be hooked");
        return 0;
    }
    ellie::Init(base, a);
    return 0;
}

void Init() {
    // hid.dll may be loaded by other programs in the game folder; they only
    // get the forwarding.
    if (!IsSupportedHost()) return;

    wchar_t ini[MAX_PATH], log[MAX_PATH];
    if (!GamePath(L"PlayableEllie.log", log, MAX_PATH)) return;
    // Settings live in Documents so updates never reset them (game folder if
    // Documents can't be found).
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, ini)) ||
        wcscat_s(ini, MAX_PATH, L"\\Playable Ellie (WORKING)\\PlayableEllie.ini") != 0) {
        if (!GamePath(L"PlayableEllie.ini", ini, MAX_PATH)) return;
    }
    settings::Init(ini);
    logx::Open(log, settings::GetBool("Logging", true));
    logx::Write(MOD_NAME " " MOD_VERSION " loaded (hid.dll)");
    if (proxy::MissingExports()) logx::Write("warning: %d hid exports not found in System32\\hid.dll", proxy::MissingExports());
    logx::Write("settings file: %ls%s", settings::Path().c_str(), settings::CreatedThisRun() ? " (created)" : settings::UpgradedThisRun() ? " (updated with the new keys)" : "");

    // Loader lock is held here: do the real work on a thread.
    HANDLE t = CreateThread(nullptr, 0, Startup, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
    else logx::Write("ERROR: could not create the startup thread (%lu)", GetLastError());
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (!proxy::LoadRealHid()) return FALSE; // without the real DLL the game cannot run
        Init();
    }
    return TRUE;
}
