#pragma once

#include <string>

void ExecuteInRobloxVM(const std::string& script);
bool InitializeBridge();
void ShutdownBridge();
