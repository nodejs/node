/* 
 * A Win32 program that scrolls and writes to the console using the ioctl-like
 * interface.
 */

#include <stdio.h>
#include <windows.h>

int main()
{
    HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);

    for (int i = 0; i < 80; ++i) {

        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(conout, &info);

        SMALL_RECT src = { 0, 1, info.dwSize.X - 1, info.dwSize.Y - 1 };
        COORD destOrigin = { 0, 0 };
        CHAR_INFO fillCharInfo = { 0 };
        fillCharInfo.Char.AsciiChar = ' ';
        fillCharInfo.Attributes = 7;
        ScrollConsoleScreenBuffer(conout,
                                  &src,
                                  NULL,
                                  destOrigin,
                                  &fillCharInfo);

        CHAR_INFO buffer = { 0 };
        buffer.Char.AsciiChar = 'X';
        buffer.Attributes = 7;
        COORD bufferSize = { 1, 1 };
        COORD bufferCoord = { 0, 0 };
        SMALL_RECT writeRegion = { 0, 0, 0, 0 };
        writeRegion.Left = writeRegion.Right = i;
        writeRegion.Top = writeRegion.Bottom = 5;
        WriteConsoleOutput(conout, 
                           &buffer, bufferSize, bufferCoord,
                           &writeRegion);

        Sleep(250);
    }
    return 0;
}
