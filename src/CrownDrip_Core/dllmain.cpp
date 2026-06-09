#include <windows.h>
#include "pipe.h"

DWORD WINAPI CoreInitialize(LPVOID)
{
    InitializeExecutionPipe();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, CoreInitialize, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
