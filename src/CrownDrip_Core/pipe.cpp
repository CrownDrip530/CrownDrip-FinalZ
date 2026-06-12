#include "pipe.h"
#include <windows.h>
#include "bridge.h"

void InitializeExecutionPipe() {
    // 將管道建立放進無窮迴圈中，確保每一次 UI 連線都是乾淨、全新的狀態
    while (true) {
        HANDLE hPipe = CreateNamedPipeA(
            "\\\\.\\pipe\\CrownDripPipe", 
            PIPE_ACCESS_INBOUND, 
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 
            1, 1024 * 64, 1024 * 64, 0, NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(500);
            continue;
        }

        // 等待 C# UI 按下 Execute 連線進來
        if (ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED)) {
            char cipherBuffer[1024 * 64] = { 0 };
            DWORD bytesRead = 0;

            // 讀取 UI 送過來的加密腳本
            if (ReadFile(hPipe, cipherBuffer, sizeof(cipherBuffer) - 1, &bytesRead, NULL)) {
                // XOR 0x5A 解密
                for (DWORD i = 0; i < bytesRead; i++) {
                    cipherBuffer[i] ^= 0x5A;
                }

                // 安全呼叫虛擬機執行
                ExecuteInRobloxVM(cipherBuffer);
            }
        }

        // 斷開並徹底釋放本次管道控制代碼，供下一次點擊使用
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}
