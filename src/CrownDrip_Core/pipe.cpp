#include "pipe.h"
#include <windows.h>
#include "bridge.h" // Add this line

void InitializeExecutionPipe() {
    HANDLE hPipe = CreateNamedPipeA("\\\\.\\pipe\\CrownDripPipe", PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE, 1, 0, 0, 0, NULL);

    while (ConnectNamedPipe(hPipe, NULL)) {
        char cipherBuffer[4096] = {0};
        DWORD bytesRead;

        if (ReadFile(hPipe, cipherBuffer, sizeof(cipherBuffer) - 1, &bytesRead, NULL)) {
            for (DWORD i = 0; i < bytesRead; i++) {
                cipherBuffer[i] ^= 0x5A;
            }
            ExecuteInRobloxVM(cipherBuffer);
        }
        DisconnectNamedPipe(hPipe);
    }
}
