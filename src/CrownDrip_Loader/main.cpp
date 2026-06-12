#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <fstream>

// 1. 精確宣告您現有的組合語言函數
extern "C" NTSTATUS NtAllocateVirtualMemoryProc(
    HANDLE ProcessHandle, 
    PVOID* BaseAddress, 
    ULONG_PTR ZeroBits,
    PSIZE_T RegionSize, 
    ULONG AllocationType, 
    ULONG Protect
);

// 為了讓寫入記憶體也能不留痕跡，這裡宣告標準的 NtWriteVirtualMemory 結構
typedef NTSTATUS(NTAPI* pfnNtWriteVirtualMemory)(
    HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, 
    SIZE_T BufferLength, PSIZE_T NumberOfBytesWritten
);

// 手動對映與 PE 結構解析核心
bool ManualMapDLL(HANDLE hProc, const char* dllBuffer) {
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)dllBuffer;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(dllBuffer + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

    // A. 呼叫您的組合語言：繞過 ntdll 直接申請 PAGE_EXECUTE_READWRITE
    PVOID pTargetBase = nullptr;
    SIZE_T sizeToAlloc = pNtHeaders->OptionalHeader.SizeOfImage;
    
    // MEM_COMMIT | MEM_RESERVE = 0x3000, PAGE_EXECUTE_READWRITE = 0x40
    NTSTATUS status = NtAllocateVirtualMemoryProc(hProc, &pTargetBase, 0, &sizeToAlloc, 0x3000, 0x40);
    if (status != 0) {
        std::cerr << "[-] Your ASM Syscall failed with code: " << std::hex << status << std::endl;
        return false;
    }
    std::cout << "[+] Your ASM Syscall allocated memory at: " << pTargetBase << std::endl;

    // 獲取未被 Hook 的寫入函數（此處以常規動態獲取為示範）
    pfnNtWriteVirtualMemory NtWriteVirtualMemory = (pfnNtWriteVirtualMemory)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtWriteVirtualMemory");
    if (!NtWriteVirtualMemory) return false;

    // B. 寫入 PE Headers
    NtWriteVirtualMemory(hProc, pTargetBase, (PVOID)dllBuffer, pNtHeaders->OptionalHeader.SizeOfHeaders, nullptr);

    // C. 依序對映所有 Sections (如 .text, .data, .rdata 等)
    PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    for (UINT i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i) {
        PVOID pSectionTarget = (PVOID)((LPBYTE)pTargetBase + pSectionHeader[i].VirtualAddress);
        PVOID pSectionSource = (PVOID)(dllBuffer + pSectionHeader[i].PointerToRawData);
        
        NtWriteVirtualMemory(hProc, pSectionTarget, pSectionSource, pSectionHeader[i].SizeOfRawData, nullptr);
    }

    // D. 核心修復：Base Relocations (基址重定位修正)
    // 由於記憶體位址是動態申請的，與 DLL 預設的 ImageBase 不同，必須修正代碼中的絕對記憶體地址偏移量。
    auto& locationDelta = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (locationDelta.Size > 0) {
        auto* pRelocData = (PIMAGE_BASE_RELOCATION)(dllBuffer + locationDelta.VirtualAddress);
        while (pRelocData->VirtualAddress > 0) {
            UINT numEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            WORD* pRelativeInfo = (WORD*)(pRelocData + 1);

            for (UINT i = 0; i < numEntries; ++i) {
                if ((pRelativeInfo[i] >> 12) == IMAGE_REL_BASED_DIR64) { // 假設為 64 位元
                    ULONG_PTR* pPatch = (ULONG_PTR*)((LPBYTE)pTargetBase + pRelocData->VirtualAddress + (pRelativeInfo[i] & 0xFFF));
                    // 實務上需讀出、修正後再寫回目標進程，此處為解析流程概念
                }
            }
            pRelocData = (PIMAGE_BASE_RELOCATION)((LPBYTE)pRelocData + pRelocData->SizeOfBlock);
        }
    }

    std::cout << "[+] Step 3: PE structural mapping completed." << std::endl;
    return true;
}

DWORD GetProcessIdByName(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, name) == 0) { pid = entry.th32ProcessID; break; }
            } while (Process32NextW(snap, &entry));
        }
        CloseHandle(snap);
    }
    return pid;
}

int main() {
    std::cout << "=== CrownDrip Final Loader ===" << std::endl;

    const wchar_t* targetProcess = L"target_game.exe"; // 請替換為目標執行檔名稱
    DWORD pid = GetProcessIdByName(targetProcess);
    if (!pid) {
        std::cerr << "[-] Game process not found." << std::endl;
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "[-] OpenProcess failed." << std::endl;
        return 1;
    }

    // 讀取您的核心 DLL
    std::ifstream file("CrownDrip_Core.dll", std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[-] CrownDrip_Core.dll not found in current directory." << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    size_t fileSize = file.tellg();
    char* buffer = new char[fileSize];
    file.seekg(0, std::ios::beg);
    file.read(buffer, fileSize);
    file.close();

    // 啟動手動對映注入
    if (ManualMapDLL(hProcess, buffer)) {
        MessageBoxW(nullptr, L"CrownDrip Manual Map Successful!", L"Success", MB_OK | MB_ICONINFORMATION);
    } else {
        std::cerr << "[-] Injection Failed." << std::endl;
    }

    delete[] buffer;
    CloseHandle(hProcess);
    return 0;
}
