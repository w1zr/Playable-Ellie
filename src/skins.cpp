// Every Extras > Skins skin unlocked and owned (UnlockSkins=1), the same way
// TLOU Outfit Changer 1.0.0 does it: the two Skins menu checks read a byte
// the mod owns instead of the debug "Unlock All Extras Content" bool, and the
// three "is this skin owned?" checks jump to small stubs that answer "owned".
// Every site is found by signature (skinscan); on any mismatch nothing is
// written. Points, saves and the profile are never touched.
#include "skins.h"

#include "log.h"
#include "skinscan.h"

#include <windows.h>
#include <cstring>
#include <vector>

namespace skins {

namespace {

// rip-relative operands reach +-2 GB, so the mod's byte and stubs must sit
// close to the image. Walk down from the image base in allocation-granularity steps.
uint8_t* AllocNear(const uint8_t* base, DWORD protect) {
    const uintptr_t start = reinterpret_cast<uintptr_t>(base) & ~static_cast<uintptr_t>(0xFFFF);
    for (uintptr_t step = 1; step < 0x6000; ++step) { // up to ~1.5 GB below
        const uintptr_t at = start - step * 0x10000;
        void* p = VirtualAlloc(reinterpret_cast<void*>(at), 0x1000, MEM_RESERVE | MEM_COMMIT, protect);
        if (p) return static_cast<uint8_t*>(p);
    }
    return nullptr;
}

bool WriteCode(uint8_t* at, const void* bytes, size_t n) {
    DWORD old = 0;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    return true;
}

bool Disp32(const uint8_t* base, const skinscan::RipRead& rd, const uint8_t* target, int32_t& out) {
    const int64_t d = reinterpret_cast<intptr_t>(target) - reinterpret_cast<intptr_t>(base + rd.end);
    out = static_cast<int32_t>(d);
    return out == d;
}

} // namespace

bool Install() {
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        logx::Write("skins: ERROR: main module has no DOS header");
        return false;
    }
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        logx::Write("skins: ERROR: main module is not a PE32+ image");
        return false;
    }
    
    // Code = first executable section; strings and the debug flag live in
    // the readable, non-executable sections.
    skinscan::Range code;
    std::vector<skinscan::Range> data;
    const auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const skinscan::Range r{sec[i].VirtualAddress, sec[i].VirtualAddress + sec[i].Misc.VirtualSize};
        const DWORD ch = sec[i].Characteristics;
        if (ch & IMAGE_SCN_MEM_EXECUTE) {
            if (!code.end) code = r;
        } else if ((ch & IMAGE_SCN_MEM_READ) && !(ch & IMAGE_SCN_MEM_DISCARDABLE)) {
            data.push_back(r);
        }
    }
    if (!code.end) {
        logx::Write("skins: ERROR: no executable section found");
        return false;
    }

    const skinscan::SkinSites s = skinscan::LocateSkinSites(base, code, data);
    if (!s.ok) {
        logx::Write("skins: ERROR: %s. skins stay as the game has them; nothing was changed.", s.error.c_str());
        return false;
    }
    logx::Write("skins: found: unlock flag RVA 0x%zX, main menu check RVA 0x%zX, in-game menu check RVA 0x%zX",
                s.unlockAll, s.mainMenu.instr, s.gameMenu.instr);
    for (const auto& o : s.owned)
        logx::Write("skins: found: %s check RVA 0x%zX", o.equip ? "equip" : "owned", o.offset);

    // Private byte the redirected reads and the stubs test. The game resets
    // its own debug block, so its flag is never written.
    uint8_t* flag = AllocNear(base, PAGE_READWRITE);
    uint8_t* stubs = AllocNear(base, PAGE_READWRITE);
    if (!flag || !stubs) {
        logx::Write("skins: ERROR: could not reserve memory near the game image. skins stay as the game has them; nothing was changed.");
        return false;
    }
    *flag = 1;

    // Build everything before writing anything, so a range error changes nothing.
    int32_t mainDisp, gameDisp;
    if (!Disp32(base, s.mainMenu, flag, mainDisp) || !Disp32(base, s.gameMenu, flag, gameDisp)) {
        logx::Write("skins: ERROR: flag out of rip-relative range. skins stay as the game has them; nothing was changed.");
        return false;
    }
    std::vector<std::vector<uint8_t>> jumps;
    size_t used = 0;
    const auto u = [&](size_t rva) { return reinterpret_cast<uint64_t>(base) + rva; };
    for (const auto& o : s.owned) {
        uint8_t* at = stubs + used;
        const auto code8 = skinscan::BuildOwnedStub(reinterpret_cast<uint64_t>(at), u(o.offset), u(o.owned),
                                                reinterpret_cast<uint64_t>(flag), o);
        const auto jump = skinscan::BuildSiteJump(u(o.offset), reinterpret_cast<uint64_t>(at));
        if (code8.empty() || jump.empty()) {
            logx::Write("skins: ERROR: stub out of range. skins stay as the game has them; nothing was changed.");
            return false;
        }
        std::memcpy(at, code8.data(), code8.size());
        used += (code8.size() + 15) & ~static_cast<size_t>(15);
        jumps.push_back(jump);
    }
    DWORD old = 0;
    if (!VirtualProtect(stubs, 0x1000, PAGE_EXECUTE_READ, &old)) {
        logx::Write("skins: ERROR: could not make stubs executable (%lu). skins stay as the game has them; nothing was changed.", GetLastError());
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), stubs, used);

    // Stubs are ready, so patch the game: menu reads first, then the jumps.
    bool ok = WriteCode(base + s.mainMenu.dispOffset, &mainDisp, 4) &&
              WriteCode(base + s.gameMenu.dispOffset, &gameDisp, 4);
    for (size_t i = 0; ok && i < s.owned.size(); ++i)
        ok = WriteCode(base + s.owned[i].offset, jumps[i].data(), jumps[i].size());
    if (!ok) {
        logx::Write("skins: ERROR: patching failed (%lu)", GetLastError());
        return false;
    }
    logx::Write("skins: installed: Skins menus open, every skin owned (UnlockSkins=1)");
    return true;
}

} // namespace skins
