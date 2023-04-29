#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "TestUtil.cc"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s x y width height\n", argv[0]);
        return 1;
    }

    const HANDLE conout = CreateFileW(L"CONOUT$",
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(conout != INVALID_HANDLE_VALUE);

    SMALL_RECT sr = {
        (short)atoi(argv[1]),
        (short)atoi(argv[2]),
        (short)(atoi(argv[1]) + atoi(argv[3]) - 1),
        (short)(atoi(argv[2]) + atoi(argv[4]) - 1),
    };

    trace("Calling SetConsoleWindowInfo with {L=%d,T=%d,R=%d,B=%d}",
        sr.Left, sr.Top, sr.Right, sr.Bottom);
    BOOL ret = SetConsoleWindowInfo(conout, TRUE, &sr);
    const unsigned lastError = GetLastError();
    const char *const retStr = ret ? "OK" : "failed";
    trace("SetConsoleWindowInfo ret: %s (LastError=0x%x)", retStr, lastError);
    printf("SetConsoleWindowInfo ret: %s (LastError=0x%x)\n", retStr, lastError);

    return 0;
}
