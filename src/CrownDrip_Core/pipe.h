#pragma once

// Entry point for your named pipe listener logic found in pipe.cpp 
void InitializeExecutionPipe(); // Listens on \\.\pipe\CrownDripPipe and decodes scripts using XOR 0x5A key 
