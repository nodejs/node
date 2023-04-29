#include <windows.h>

#include <assert.h>
#include <locale.h>
#include <stdio.h>

#include <iostream>

int main() {
    setlocale(LC_ALL, "");

    OSVERSIONINFOEXW info = {0};
    info.dwOSVersionInfoSize = sizeof(info);
    assert(GetVersionExW((OSVERSIONINFOW*)&info));

    printf("dwMajorVersion      = %d\n", (int)info.dwMajorVersion);
    printf("dwMinorVersion      = %d\n", (int)info.dwMinorVersion);
    printf("dwBuildNumber       = %d\n", (int)info.dwBuildNumber);
    printf("dwPlatformId        = %d\n", (int)info.dwPlatformId);
    printf("szCSDVersion        = %ls\n", info.szCSDVersion);
    printf("wServicePackMajor   = %d\n", info.wServicePackMajor);
    printf("wServicePackMinor   = %d\n", info.wServicePackMinor);
    printf("wSuiteMask          = 0x%x\n", (unsigned int)info.wSuiteMask);
    printf("wProductType        = 0x%x\n", (unsigned int)info.wProductType);

    return 0;
}
