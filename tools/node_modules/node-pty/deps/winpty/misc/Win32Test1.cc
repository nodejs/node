#define _WIN32_WINNT 0x0501
#include "../src/shared/DebugClient.cc"
#include <windows.h>
#include <stdio.h>

const int SC_CONSOLE_MARK = 0xFFF2;

CALLBACK DWORD writerThread(void*)
{
    while (true) {
        Sleep(1000);
        trace("writing");
        printf("X\n");
        trace("written");
    }
}

int main()
{
    CreateThread(NULL, 0, writerThread, NULL, 0, NULL);
    trace("marking console");
    HWND hwnd = GetConsoleWindow();
    PostMessage(hwnd, WM_SYSCOMMAND, SC_CONSOLE_MARK, 0);

    Sleep(2000);

    trace("reading output");
    CHAR_INFO buf[1];
    COORD bufSize = { 1, 1 };
    COORD zeroCoord = { 0, 0 };
    SMALL_RECT readRect = { 0, 0, 0, 0 };
    ReadConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE),
                      buf,
                      bufSize,
                      zeroCoord,
                      &readRect);
    trace("done reading output");

    Sleep(2000);

    PostMessage(hwnd, WM_CHAR, 27, 0x00010001);

    Sleep(1100);

    return 0;
}
