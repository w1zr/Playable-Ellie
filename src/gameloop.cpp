#include "gameloop.h"

#include "log.h"

#include "../third_party/minhook/include/MinHook.h"

#include <windows.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

namespace gameloop {

namespace {

using GameLoopFn = void(__fastcall*)(uint64_t);
using Callback = bool(__fastcall*)(void* item, int message);
using PlayerLookup = void*(__fastcall*)(const void* handle);

constexpr int kExecute = 5; // developer-menu message: run / toggle

const uint8_t* g_base = nullptr;
GameLoopFn oGameLoop = nullptr;
std::atomic<void (*)()> g_tick{nullptr};
std::mutex g_queueLock;
std::vector<std::function<void()>> g_queue;
std::atomic<bool> g_running{false};

PlayerLookup g_playerLookup = nullptr;
const void* g_playerHandle = nullptr;

// Developer-menu callbacks read their argument from the item at +0x60.
struct alignas(16) FakeItem {
    uint8_t bytes[0x100];
};

bool CallGuarded(Callback cb, void* item, int message) {
    __try {
        cb(item, message);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __fastcall HkGameLoop(uint64_t arg) {
    if (!g_running.exchange(true)) logx::Write("game loop hook running");
    if (auto tick = g_tick.load()) tick();
    std::vector<std::function<void()>> work;
    {
        std::lock_guard<std::mutex> l(g_queueLock);
        work.swap(g_queue);
    }
    for (auto& fn : work) fn();
    oGameLoop(arg);
}

} // namespace

bool Install(const uint8_t* base, size_t loop, size_t playerHandle, size_t playerLookup) {
    g_base = base;
    g_playerHandle = base + playerHandle;
    g_playerLookup = reinterpret_cast<PlayerLookup>(const_cast<uint8_t*>(base + playerLookup));
    void* target = const_cast<uint8_t*>(base + loop);
    const bool ok = MH_CreateHook(target, reinterpret_cast<void*>(&HkGameLoop), reinterpret_cast<void**>(&oGameLoop)) == MH_OK &&
                    MH_EnableHook(target) == MH_OK;
    logx::Write("game loop: GameLoopUpdate at RVA 0x%zX %s", loop, ok ? "hooked" : "could NOT be hooked");
    return ok;
}

void SetTick(void (*tick)()) { g_tick = tick; }

void RunOnGameThread(std::function<void()> fn) {
    std::lock_guard<std::mutex> l(g_queueLock);
    if (g_queue.size() < 64) g_queue.push_back(std::move(fn));
}

bool PlayerPresent() {
    if (!g_playerLookup) return false;
    void* p = nullptr;
    __try {
        p = g_playerLookup(g_playerHandle);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return p != nullptr;
}

bool RunDevCallback(const uint8_t* callback, uint64_t data) {
    FakeItem item{};
    std::memcpy(item.bytes + 0x60, &data, sizeof(data));
    if (!CallGuarded(reinterpret_cast<Callback>(const_cast<uint8_t*>(callback)), &item, kExecute)) {
        logx::Write("ERROR: exception in developer-menu callback RVA 0x%zX", static_cast<size_t>(callback - g_base));
        return false;
    }
    return true;
}

} // namespace gameloop
