/*
 * This test demonstrates that putting a console into selection mode does not
 * block the low-level console APIs, even though it blocks WriteFile.
 */

#define _WIN32_WINNT 0x0501
#include "../src/shared/DebugClient.cc"
#include <windows.h>
#include <stdio.h>

const int SC_CONSOLE_MARK = 0xFFF2;

CALLBACK DWORD writerThread(void*)
{
    CHAR_INFO xChar, fillChar;
    memset(&xChar, 0, sizeof(xChar));
    xChar.Char.AsciiChar = 'X';
    xChar.Attributes = 7;
    memset(&fillChar, 0, sizeof(fillChar));
    fillChar.Char.AsciiChar = ' ';
    fillChar.Attributes = 7;
    COORD oneCoord = { 1, 1 };
    COORD zeroCoord = { 0, 0 };

    while (true) {
        SMALL_RECT writeRegion = { 5, 5, 5, 5 };
        WriteConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE),
                           &xChar, oneCoord,
                           zeroCoord,
                           &writeRegion);
        Sleep(500);
        SMALL_RECT scrollRect = { 1, 1, 20, 20 };
        COORD destCoord = { 0, 0 };
        ScrollConsoleScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE),
                                  &scrollRect,
                                  NULL,
                                  destCoord,
                                  &fillChar);
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
