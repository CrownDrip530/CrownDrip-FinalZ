#include "bridge.h"
#include "scanner.h"
#include <windows.h>

// 宣告外部彙編函式（來自 syscalls.asm）
extern "C" NTSTATUS SysAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);

// 全域函式指針，用來存放動態抓取到的 Luau 進入點
t_luau_load rbx_luau_load = nullptr;
t_luau_pcall rbx_luau_pcall = nullptr;
rbx_LuaState global_L = nullptr;

// 初始化橋接器，利用特徵碼動態對齊記憶體位址
bool InitializeBridge() {
    // 1. 盲掃 Luau 編譯與執行函式的當週特徵碼
    uintptr_t load_addr = ScanMemoryPattern("\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x57\x41\x56\x41\x57\x48\x83\xEC\x40", "xxxxxxxxxxxxxxxxxxxxxxxx");
    uintptr_t pcall_addr = ScanMemoryPattern("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x30\x48\x8B\xF1\x8B\xFA", "xxxxxxxxxxxxxxxxxxxx");
    uintptr_t state_addr = ScanMemoryPattern("\x48\x8B\x05\x00\x00\x00\x00\x48\x8B\x88\x00\x00\x00\x00\x48\x85xC9", "xxx????xxx????xxx");

    if (!load_addr || !pcall_addr || !state_addr) return false;

    // 2. 將記憶體地址轉換為可執行的 C++ 函式指針
    rbx_luau_load = (t_luau_load)load_addr;
    rbx_luau_pcall = (t_luau_pcall)pcall_addr;

    // 3. 解除動態指針，獲取當前遊戲主執行緒的核心 LuauState
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    int32_t offset = *(int32_t*)(state_addr + 3);
    global_L = *(rbx_LuaState*)(state_addr + 7 + offset);

    return true;
}

// 執行來自具名管道的腳本
void ExecuteInRobloxVM(const std::string& decryptedScript) {
    if (!global_L && !InitializeBridge()) return;

    // 呼叫動態獲取的 Luau 編譯器，將純文字的 Lua 腳本編譯為字節碼
    if (rbx_luau_load(global_L, "=CrownDripChunk", decryptedScript.c_str(), decryptedScript.size(), 0) == 0) {
        // 編譯成功後，呼叫受保護的 pcall 執行該腳本區塊
        rbx_luau_pcall(global_L, 0, 0, 0);
    }
}
