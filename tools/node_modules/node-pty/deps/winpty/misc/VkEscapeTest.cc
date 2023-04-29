/*
 * Sending VK_PAUSE to the console window almost works as a mechanism for
 * pausing it, but it doesn't because the console could turn off the
 * ENABLE_LINE_INPUT console mode flag.
 */

#define _WIN32_WINNT 0x0501
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

CALLBACK DWORD pausingThread(LPVOID dummy)
{
    if (1) {
        Sleep(1000);
        HWND hwnd = GetConsoleWindow();
        SendMessage(hwnd, WM_KEYDOWN, VK_PAUSE, 1);
        Sleep(1000);
        SendMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 1);
    }

    if (0) {
        INPUT_RECORD ir;
        memset(&ir, 0, sizeof(ir));
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.wVirtualKeyCode = VK_PAUSE;
        ir.Event.KeyEvent.wRepeatCount = 1;
    }

    return 0;
}

int main()
{
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD c = { 0, 0 };

    DWORD mode;
    GetConsoleMode(hin, &mode);
    SetConsoleMode(hin, mode &
                   ~(ENABLE_LINE_INPUT));

    CreateThread(NULL, 0,
                 pausingThread, NULL,
                 0, NULL);

    int i = 0;
    while (true) {
        Sleep(100);
        printf("%d\n", ++i);
    }

    return 0;
}
