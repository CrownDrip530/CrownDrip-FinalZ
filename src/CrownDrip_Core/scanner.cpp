#include "scanner.h"

uintptr_t ScanMemoryPattern(const char* pattern, const char* mask) {
    uintptr_t baseAddress = (uintptr_t)GetModuleHandleA(NULL); // 獲取 Roblox 模組基底
    size_t searchSize = 0x5000000; // 掃描前 80MB 記憶體區段
    size_t patternLen = strlen(mask);

    for (size_t i = 0; i < searchSize - patternLen; i++) {
        bool isMatch = true;
        for (size_t j = 0; j < patternLen; j++) {
            if (mask[j] != '?' && pattern[j] != *(char*)(baseAddress + i + j)) {
                isMatch = false;
                break;
            }
        }
        if (isMatch) return baseAddress + i; // 找到當週最新的 Luau 函式進入點
    }
    return 0;
}
