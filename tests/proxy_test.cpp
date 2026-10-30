// Offline test for the hid.dll proxy: every export of System32\hid.dll must
// exist in build\hid.dll with the same name and ordinal, and calls made
// through the proxy must reach the real DLL. Loading the proxy here is safe:
// the mod itself only starts inside tlou-i.exe / tlou-i-l.exe.
//
// usage: proxy_test.exe [path to proxy hid.dll]   (default build\hid.dll)
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (cond) { std::printf("  PASS  "); }               \
        else { std::printf("  FAIL  "); ++g_failures; }      \
        std::printf(__VA_ARGS__);                            \
        std::printf("\n");                                   \
    } while (0)

// ordinal -> name ("" for ordinal-only), read from the file on disk.
static bool ReadExports(const wchar_t* path, std::map<WORD, std::string>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(file.data());
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    auto rvaToOff = [&](DWORD rva) -> size_t {
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
            if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + sec[i].SizeOfRawData)
                return rva - sec[i].VirtualAddress + sec[i].PointerToRawData;
        return 0;
    };
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return false;
    auto* ed = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(file.data() + rvaToOff(dir.VirtualAddress));
    auto* funcs = reinterpret_cast<DWORD*>(file.data() + rvaToOff(ed->AddressOfFunctions));
    auto* names = reinterpret_cast<DWORD*>(file.data() + rvaToOff(ed->AddressOfNames));
    auto* ords = reinterpret_cast<WORD*>(file.data() + rvaToOff(ed->AddressOfNameOrdinals));
    for (DWORD i = 0; i < ed->NumberOfFunctions; ++i)
        if (funcs[i]) out[static_cast<WORD>(ed->Base + i)] = "";
    for (DWORD i = 0; i < ed->NumberOfNames; ++i)
        out[static_cast<WORD>(ed->Base + ords[i])] = reinterpret_cast<const char*>(file.data() + rvaToOff(names[i]));
    return true;
}

int wmain(int argc, wchar_t** argv) {
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    lstrcatW(sys, L"\\hid.dll");
    const wchar_t* proxyPath = argc > 1 ? argv[1] : L"build\\hid.dll";

    std::printf("exports\n");
    std::map<WORD, std::string> real, proxy;
    CHECK(ReadExports(sys, real), "read System32 hid.dll exports (%zu)", real.size());
    CHECK(ReadExports(proxyPath, proxy), "read proxy exports (%zu)", proxy.size());
    int missing = 0, renamed = 0;
    for (const auto& [ord, name] : real) {
        auto it = proxy.find(ord);
        if (it == proxy.end()) ++missing;
        else if (it->second != name) ++renamed;
    }
    CHECK(missing == 0 && renamed == 0 && proxy.size() == real.size(),
          "same names and ordinals (%d missing, %d different, %zu extra)", missing, renamed,
          proxy.size() > real.size() ? proxy.size() - real.size() : 0);

    std::printf("forwarding\n");
    HMODULE p = LoadLibraryW(proxyPath);
    CHECK(p != nullptr, "proxy loads (%lu)", p ? 0 : GetLastError());
    HMODULE r = GetModuleHandleW(sys);
    CHECK(r != nullptr && r != p, "proxy loaded the real System32 hid.dll");
    if (!p || !r) return 1;

    // HidD_GetHidGuid(GUID*): no device needed, the same GUID both ways.
    using G = void(WINAPI*)(GUID*);
    auto pg = reinterpret_cast<G>(GetProcAddress(p, "HidD_GetHidGuid"));
    auto rg = reinterpret_cast<G>(GetProcAddress(r, "HidD_GetHidGuid"));
    GUID a{}, b{};
    if (pg && rg) {
        pg(&a);
        rg(&b);
    }
    CHECK(pg && rg && pg != rg && std::memcmp(&a, &b, sizeof a) == 0 && a.Data1 != 0, "HidD_GetHidGuid through the proxy");
    // Every function the game imports resolves through the proxy.
    int found = 0;
    for (const char* n : {"HidD_GetHidGuid", "HidD_GetAttributes", "HidP_GetValueCaps", "HidD_FreePreparsedData",
                          "HidD_GetFeature", "HidD_SetFeature", "HidD_GetManufacturerString", "HidD_GetProductString",
                          "HidD_GetSerialNumberString", "HidP_GetCaps", "HidD_GetPreparsedData"}) {
        FARPROC pp = GetProcAddress(p, n), rp = GetProcAddress(r, n);
        if (pp && rp && pp != rp) ++found;
    }
    CHECK(found == 11, "all 11 hid functions tlou-i.exe imports are proxied (%d)", found);

    std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "ALL PASSED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
