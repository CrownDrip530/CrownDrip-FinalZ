#include <windows.h>
// Assuming you have custom syscall wrappers for stealth allocation [1]
extern "C" NTSTATUS NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);

BOOL ManualMapDLL(DWORD targetPID) 
{
    // 1. Open the DLL file locally (e.g., CrownDrip_Core.dll from your build output) [2]
    HANDLE hFile = CreateFileA("CrownDrip_Core.dll", GENERIC_READ | FILE_SHARE_READ, NULL, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); 
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD fileSizeHigh = 0;
    SIZE_T dllSize = GetFileSize(hFile, &fileSizeHigh);
    
    // Allocate local memory to read and parse the PE [1]
    PVOID localBuffer = VirtualAlloc(NULL, (SIZE_T)dllSize + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); 
    DWORD bytesRead; 
    
    if (!ReadFile(hFile, localBuffer, (DWORD)dllSize, &bytesRead, NULL) || bytesRead != dllSize) {
        CloseHandle(hFile); return FALSE; 
    }

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)localBuffer; 
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)dosHeader + dosHeader->e_lfanew);
    
    // 2. Allocate memory in the target process using Syscalls to bypass hooks [1]
    PVOID remoteBaseAddress = NULL;
    SIZE_T allocSize = ntHeaders->OptionalHeader.SizeOfImage; 
    NtAllocateVirtualMemory((HANDLE)-1, &remoteBaseAddress, 0, (PSIZE_T)&allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    // Copy Header to remote memory first
    WriteProcessMemory(..., remoteBaseAddress, localBuffer, ntHeaders->OptionalHeader.SizeOfHeaders, ...); 

    // Loop through all sections and map them [3]
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders); 
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        WriteProcessMemory(..., (PVOID)((DWORD_PTR)remoteBaseAddress + section[i].VirtualAddress), localBuffer + section[i].PointerToRawData, section[i].SizeOfRawData, ...); 
    }

    // [Import Resolution Logic - Iterating through Import Directory]
    PIMAGE_DATA_DIRECTORY importDir = &ntHeaders->OptionalHeader.DataDirectory[1]; 
    if (importDir->VirtualAddress) {
        PIMAGE_IMPORT_DESCRIPTOR imports = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)remoteBaseAddress + importDir->VirtualAddress);
        
        while (imports->Name != 0 && imports->OriginalFirstThunk != 0) {
            // Get the DLL name and load it locally to find addresses [3]
            char* dllName = (char*)((DWORD_PTR)localBuffer + imports->Name); 
            HMODULE hMod = LoadLibraryA(dllName); 
            
            PIMAGE_THUNK_DATA originalThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)remoteBaseAddress + imports->OriginalFirstThunk);
            PIMAGE_THUNK_DATA firstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)remoteBaseAddress + imports->FirstThunk);

            while (originalThunk->u1.Function != 0 && originalThunk->u1.AddressOfData != 0) { 
                // Resolve function pointer [3]
                PIMAGE_IMPORT_BY_NAME impName = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)localBuffer + originalThunk->u1.AddressOfData);
                FARPROC pFuncAddress = GetProcAddress(hMod, impName->Name);

                WriteProcessMemory(..., &firstThunk->u1.Function, &pFuncAddress, sizeof(pFuncAddress), ...); 
                
                ++originalThunk; 
                ++firstThunk; 
            }
            ++imports; 
        }
    }

    // Jump to entry point [3] (Use
