#pragma once
#include <string>

// Type definitions inferred from usage in bridge.cpp
typedef void* rbx_LuaState;

// Function pointer types based on how they are called with Luau signatures:
using t_luau_load = int(*)(rbx_LuaState L, const char* chunkName, const char* sourceCode, size_t codeSize, int unknown);
using t_luau_pcall = void(*)(rbx_LuaState L, int
