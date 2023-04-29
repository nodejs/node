//
// Verify that console selection blocks writes to an inactive console screen
// buffer.  Writes TEST PASSED or TEST FAILED to the popup console window.
//

#include <windows.h>
#include <stdio.h>

#include <string>

#include "TestUtil.cc"

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;

bool g_useMark = false;

CALLBACK DWORD pausingThread(LPVOID dummy)
{
    HWND hwnd = GetConsoleWindow();
    trace("Sending selection to freeze");
    SendMessage(hwnd, WM_SYSCOMMAND,
        g_useMark ? SC_CONSOLE_MARK :
                    SC_CONSOLE_SELECT_ALL,
        0);
    Sleep(1000);
    trace("Sending escape WM_CHAR to unfreeze");
    SendMessage(hwnd, WM_CHAR, 27, 0x00010001);
    Sleep(1000);
}

static HANDLE createBuffer() {
    HANDLE buf = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL);
    ASSERT(buf != INVALID_HANDLE_VALUE);
    return buf;
}

static void runTest(bool useMark, bool createEarly) {
    trace("=======================================");
    trace("useMark=%d createEarly=%d", useMark, createEarly);
    g_useMark = useMark;
    HANDLE buf = INVALID_HANDLE_VALUE;

    if (createEarly) {
        buf = createBuffer();
    }

    CreateThread(NULL, 0,
         pausingThread, NULL,
         0, NULL);
    Sleep(500);

    if (!createEarly) {
        trace("Creating buffer");
        TimeMeasurement tm1;
        buf = createBuffer();
        const double elapsed1 = tm1.elapsed();
        if (elapsed1 >= 0.250) {
            printf("!!! TEST FAILED !!!\n");
            Sleep(2000);
            return;
        }
    }

    trace("Writing to aux buffer");
    TimeMeasurement tm2;
    DWORD actual = 0;
    BOOL ret = WriteConsoleW(buf, L"HI", 2, &actual, NULL);
    const double elapsed2 = tm2.elapsed();
    trace("Writing to aux buffer: finished: ret=%d actual=%d (elapsed=%1.3f)", ret, actual, elapsed2);
    if (elapsed2 < 0.250) {
        printf("!!! TEST FAILED !!!\n");
    } else {
        printf("TEST PASSED\n");
    }
    Sleep(2000);
}

int main(int argc, char **argv) {
    if (argc == 1) {
        startChildProcess(L"child");
        return 0;
    }

    std::string arg = argv[1];
    if (arg == "child") {
        for (int useMark = 0; useMark <= 1; useMark++) {
            for (int createEarly = 0; createEarly <= 1; createEarly++) {
                runTest(useMark, createEarly);
            }
        }
        printf("done...\n");
        Sleep(1000);
    }
    return 0;
}
