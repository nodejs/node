#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "TestUtil.cc"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s x y width height\n", argv[0]);
        return 1;
    }

    const HANDLE conout = CreateFileW(L"CONOUT$",
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(conout != INVALID_HANDLE_VALUE);

    COORD size = {
        (short)atoi(argv[1]),
        (short)atoi(argv[2]),
    };

    BOOL ret = SetConsoleScreenBufferSize(conout, size);
    const unsigned lastError = GetLastError();
    const char *const retStr = ret ? "OK" : "failed";
    trace("SetConsoleScreenBufferSize ret: %s (LastError=0x%x)", retStr, lastError);
    printf("SetConsoleScreenBufferSize ret: %s (LastError=0x%x)\n", retStr, lastError);

    return 0;
}
