#include "pipe.h"
#include "bridge.h"

#include <iostream>
#include <string>

void InitializeExecutionPipe()
{
    std::cout << "CrownDrip_Core safe build: execution pipe disabled." << std::endl;
}

void ExecuteInRobloxVM(const std::string& script)
{
    std::cout << "CrownDrip_Core safe build: script execution disabled." << std::endl;
    std::cout << "Received script length: " << script.size() << std::endl;
}
