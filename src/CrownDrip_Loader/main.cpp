#include <windows.h>

#include <iostream>

int main()
{
    std::cout << "CrownDrip Loader - Safe Build" << std::endl;
    std::cout << "External process modification and DLL manual mapping are disabled." << std::endl;

    MessageBoxW(
        nullptr,
        L"CrownDrip Loader safe build.\n\nDLL loading/manual mapping is disabled.",
        L"CrownDrip Loader",
        MB_OK | MB_ICONINFORMATION
    );

    return 0;
}
