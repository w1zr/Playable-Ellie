// hid.dll proxy. The game imports HID.DLL, it is not a KnownDLL, and Windows
// looks in the game folder before System32, so dropping this DLL there loads
// the mod with no third-party loader. No other TLOU mod uses this name (w1z's
// menu is winmm.dll, the ammo mod version.dll, the Outfit Changer
// xinput1_4.dll, TLOU Co-op avrt.dll). Every export (same names and
// ordinals, see hid.def) is a stub in hid_stubs.asm that jumps through
// g_hidTable to the real System32 hid.dll.
//
// The other executables in the game folder load this DLL too if they import
// hid.dll, so forwarding always works; the mod itself only starts in
// tlou-i.exe / tlou-i-l.exe.
#include "proxy_hid.h"

#include <windows.h>

extern "C" void* g_hidTable[];

namespace {

struct Export {
    const char* name; // nullptr = ordinal-only
    WORD ordinal;
};

const Export kExports[] = {
#include "hid_exports.inc"
};

// Used for an export the system DLL lacks (a future Windows could drop one).
extern "C" UINT_PTR WINAPI MissingExport() {
    SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

int g_missing = 0;

} // namespace

namespace proxy {

bool LoadRealHid() {
    wchar_t path[MAX_PATH];
    const UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 12) return false;
    lstrcatW(path, L"\\hid.dll");
    // Runs from DllMain, before anything can call an export. hid's own
    // dependencies are system DLLs that are already loaded at this point.
    HMODULE real = LoadLibraryW(path);
    for (size_t i = 0; i < sizeof(kExports) / sizeof(kExports[0]); ++i) {
        FARPROC p = nullptr;
        if (real) p = GetProcAddress(real, kExports[i].name ? kExports[i].name : MAKEINTRESOURCEA(kExports[i].ordinal));
        if (!p) ++g_missing;
        g_hidTable[i] = p ? reinterpret_cast<void*>(p) : reinterpret_cast<void*>(&MissingExport);
    }
    return real != nullptr;
}

int MissingExports() { return g_missing; }

} // namespace proxy
