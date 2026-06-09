#include "scanner.h"
#include <windows.h> // Add this line
#include <cstring>   // Add this line

uintptr_t ScanMemoryPattern(const char* pattern, const char* mask) {
    uintptr_t baseAddress = (uintptr_t)GetModuleHandleA(NULL);
    size_t searchSize = 0x5000000;
    size_t patternLen = strlen(mask);

    for (size_t i = 0; i < searchSize - patternLen; i++) {
        bool isMatch = true;
        for (size_t j = 0; j < patternLen; j++) {
            if (mask[j] != '?' && pattern[j] != *(char*)(baseAddress + i + j)) {
                isMatch = false;
                break;
            }
        }
        if (isMatch) return baseAddress + i;
    }
    return 0;
}
