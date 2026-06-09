#include "pipe.h"
#include <windows.h>

void InitializeExecutionPipe() {
    HANDLE hPipe = CreateNamedPipeA("\\\\.\\pipe\\CrownDripPipe", PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE, 1, 0, 0, 0, NULL);

    while (ConnectNamedPipe(hPipe, NULL)) {
        char cipherBuffer[4096] = {0};
        DWORD bytesRead;

        if (ReadFile(hPipe, cipherBuffer, sizeof(cipherBuffer) - 1, &bytesRead, NULL)) {
            // 記憶體內即時 XOR 解密
            for (DWORD i = 0; i < bytesRead; i++) {
                cipherBuffer[i] ^= 0x5A; 
            }
            // 將解密後的 Lua 腳本強行塞入 Roblox 的主 Luau 執行緒執行
            ExecuteInRobloxVM(cipherBuffer); 
        }
        DisconnectNamedPipe(hPipe);
    }
}
