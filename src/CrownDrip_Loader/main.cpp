#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <string>

// 根據進程名稱獲取 Process ID (PID)
DWORD GetProcessIdByName(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName) == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

int main()
{
    std::cout << "=== CrownDrip Loader - Active Build ===" << std::endl;

    // ────────────────────────────────────────────────────────
    // 1. 設定您的目標進程與 DLL 的絕對路徑
    // ────────────────────────────────────────────────────────
    const wchar_t* targetProcess = L"target_game.exe";  // 填入您的目標遊戲或程式名稱
    const wchar_t* dllPath = L"C:\\Path\\To\\Your\\CrownDrip_Core.dll"; // 填入 DLL 的「絕對路徑」

    std::cout << "[*] Searching for target process..." << std::endl;
    DWORD pid = GetProcessIdByName(targetProcess);
    
    if (pid == 0) {
        std::cerr << "[-] Target process not found!" << std::endl;
        MessageBoxW(nullptr, L"Target process not found!\nMake sure the game is running.", L"CrownDrip Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    std::cout << "[+] Found target process. PID: " << pid << std::endl;

    // ────────────────────────────────────────────────────────
    // 2. 開啟目標進程並獲取記憶體操作權限
    // ────────────────────────────────────────────────────────
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        std::cerr << "[-] Failed to open target process! Error: " << GetLastError() << std::endl;
        MessageBoxW(nullptr, L"Failed to open target process!\nTry running this loader as Administrator.", L"CrownDrip Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ────────────────────────────────────────────────────────
    // 3. 在目標進程內配置一塊記憶體空間，用來存放 DLL 的路徑字串
    // ────────────────────────────────────────────────────────
    size_t pathLength = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID pRemoteBuf = VirtualAllocEx(hProcess, nullptr, pathLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteBuf) {
        std::cerr << "[-] Failed to allocate memory in target process!" << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    // ────────────────────────────────────────────────────────
    // 4. 將 DLL 路徑寫入目標進程配置好的記憶體中
    // ────────────────────────────────────────────────────────
    if (!WriteProcessMemory(hProcess, pRemoteBuf, dllPath, pathLength, nullptr)) {
        std::cerr << "[-] Failed to write DLL path to target process!" << std::endl;
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // ────────────────────────────────────────────────────────
    // 5. 獲取 Windows 標準核心 LoadLibraryW 的記憶體位址
    // ────────────────────────────────────────────────────────
    PTHREAD_START_ROUTINE pLoadLibrary = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibrary) {
        std::cerr << "[-] Failed to get LoadLibraryW address!" << std::endl;
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // ────────────────────────────────────────────────────────
    // 6. 在目標進程中建立遠端執行緒，強迫它執行 LoadLibraryW 載入 DLL
    // ────────────────────────────────────────────────────────
    std::cout << "[*] Injecting DLL into target process..." << std::endl;
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, pLoadLibrary, pRemoteBuf, 0, nullptr);
    
    if (!hThread) {
        std::cerr << "[-] CreateRemoteThread failed! Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // 等待注入線程執行完畢並清理資源
    WaitForSingleObject(hThread, INFINITE);
    
    std::cout << "[+] Injection successful!" << std::endl;
    MessageBoxW(nullptr, L"CrownDrip Loader successfully injected the DLL!", L"CrownDrip Loader", MB_OK | MB_ICONINFORMATION);

    // 清理在目標進程中申請的暫存記憶體與控制代碼
    VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return 0;
}
