#include <windows.h>
#include "pipe.h"

DWORD WINAPI CoreInitialize(LPVOID lpParam) {
    // 1. 抹除記憶體中的 PE 頭特徵，防止 Hyperion 的記憶體掃描器（Module Sweep）抓到 DLL 結構
    DWORD oldProtect;
    VirtualProtect(GetModuleHandleA(NULL), 0x1000, PAGE_READWRITE, &oldProtect);
    RtlZeroMemory(GetModuleHandleA(NULL), 0x1000); 

    // 2. 開啟背景腳本監聽管道
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitializeExecutionPipe, NULL, 0, NULL);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        CreateThread(NULL, 0, CoreInitialize, NULL, 0, NULL);
    }
    return TRUE;
}
