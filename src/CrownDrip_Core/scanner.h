#pragma once
#include <cstdint> // Required for uintptr_t definition [2] (from previous session knowledge of the file)

// Function to scan process memory based on byte signatures found in scanner.cpp 
uintptr_t ScanMemoryPattern(const char* pattern, const char* mask); 
