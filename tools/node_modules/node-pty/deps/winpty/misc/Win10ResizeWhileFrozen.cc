/*
 * Demonstrates a conhost hang that occurs when widening the console buffer
 * while selection is in progress.  The problem affects the new Windows 10
 * console, not the "legacy" console mode that Windows 10 also includes.
 *
 * First tested with:
 *  - Windows 10.0.10240
 *  - conhost.exe version 10.0.10240.16384
 *  - ConhostV1.dll version 10.0.10240.16384
 *  - ConhostV2.dll version 10.0.10240.16391
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "TestUtil.cc"

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    setWindowPos(0, 0, 1, 1);
    setBufferSize(80, 25);
    setWindowPos(0, 0, 80, 25);

    countDown(5);

    SendMessage(GetConsoleWindow(), WM_SYSCOMMAND, SC_CONSOLE_SELECT_ALL, 0);
    Sleep(2000);

    // This API call does not return.  In the console window, the "Select All"
    // operation appears to end.  The console window becomes non-responsive,
    // and the conhost.exe process must be killed from the Task Manager.
    // (Killing this test program or closing the console window is not
    // sufficient.)
    //
    // The same hang occurs whether line resizing is off or on.  It happens
    // with both "Mark" and "Select All".  Calling setBufferSize with the
    // existing buffer size does not hang, but calling it with only a changed
    // buffer height *does* hang.  Calling setWindowPos does not hang.
    setBufferSize(120, 25);

    printf("Done...\n");
    Sleep(2000);
}
