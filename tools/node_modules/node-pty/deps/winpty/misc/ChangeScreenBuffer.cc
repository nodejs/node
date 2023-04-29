// A test program for CreateConsoleScreenBuffer / SetConsoleActiveScreenBuffer
//

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <cassert>

#include "TestUtil.cc"

int main()
{
    HANDLE origBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE childBuffer = CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, CONSOLE_TEXTMODE_BUFFER, NULL);

    SetConsoleActiveScreenBuffer(childBuffer);

    while (true) {
        char buf[1024];
        CONSOLE_SCREEN_BUFFER_INFO info;

        assert(GetConsoleScreenBufferInfo(origBuffer, &info));
        trace("child.size=(%d,%d)", (int)info.dwSize.X, (int)info.dwSize.Y);
        trace("child.cursor=(%d,%d)", (int)info.dwCursorPosition.X, (int)info.dwCursorPosition.Y);
        trace("child.window=(%d,%d,%d,%d)",
            (int)info.srWindow.Left, (int)info.srWindow.Top,
            (int)info.srWindow.Right, (int)info.srWindow.Bottom);
        trace("child.maxSize=(%d,%d)", (int)info.dwMaximumWindowSize.X, (int)info.dwMaximumWindowSize.Y);

        int ch = getch();
        sprintf(buf, "%02x\n", ch);
        DWORD actual = 0;
        WriteFile(childBuffer, buf, strlen(buf), &actual, NULL);
        if (ch == 0x1b/*ESC*/ || ch == 0x03/*CTRL-C*/)
            break;

        if (ch == 'b') {
            setBufferSize(origBuffer, 40, 25);
        } else if (ch == 'w') {
            setWindowPos(origBuffer, 1, 1, 38, 23);
        } else if (ch == 'c') {
            setCursorPos(origBuffer, 10, 10);
        }
    }

    SetConsoleActiveScreenBuffer(origBuffer);

    return 0;
}
