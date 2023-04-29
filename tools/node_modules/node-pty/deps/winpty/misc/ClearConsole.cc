/*
 * Demonstrates that console clearing sets each cell's character to SP, not
 * NUL, and it sets the attribute of each cell to the current text attribute.
 *
 * This confirms the MSDN instruction in the "Clearing the Screen" article.
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms682022(v=vs.85).aspx
 * It advises using GetConsoleScreenBufferInfo to get the current text
 * attribute, then FillConsoleOutputCharacter and FillConsoleOutputAttribute to
 * write to the console buffer.
 */

#include <windows.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "TestUtil.cc"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(conout, 0x24);
    system("cls");

    setWindowPos(0, 0, 1, 1);
    setBufferSize(80, 25);
    setWindowPos(0, 0, 80, 25);

    CHAR_INFO buf;
    COORD bufSize = { 1, 1 };
    COORD bufCoord = { 0, 0 };
    SMALL_RECT rect = { 5, 5, 5, 5 };
    BOOL ret;
    DWORD actual;
    COORD writeCoord = { 5, 5 };

    // After cls, each cell's character is a space, and its attributes are the
    // default text attributes.
    ret = ReadConsoleOutputW(conout, &buf, bufSize, bufCoord, &rect);
    assert(ret && buf.Char.UnicodeChar == L' ' && buf.Attributes == 0x24);

    // Nevertheless, it is possible to change a cell to NUL.
    ret = FillConsoleOutputCharacterW(conout, L'\0', 1, writeCoord, &actual);
    assert(ret && actual == 1);
    ret = ReadConsoleOutputW(conout, &buf, bufSize, bufCoord, &rect);
    assert(ret && buf.Char.UnicodeChar == L'\0' && buf.Attributes == 0x24);

    // As well as a 0 attribute.  (As one would expect, the cell is
    // black-on-black.)
    ret = FillConsoleOutputAttribute(conout, 0, 1, writeCoord, &actual);
    assert(ret && actual == 1);
    ret = ReadConsoleOutputW(conout, &buf, bufSize, bufCoord, &rect);
    assert(ret && buf.Char.UnicodeChar == L'\0' && buf.Attributes == 0);
    ret = FillConsoleOutputCharacterW(conout, L'X', 1, writeCoord, &actual);
    assert(ret && actual == 1);
    ret = ReadConsoleOutputW(conout, &buf, bufSize, bufCoord, &rect);
    assert(ret && buf.Char.UnicodeChar == L'X' && buf.Attributes == 0);

    // The 'X' is invisible.
    countDown(3);

    ret = FillConsoleOutputAttribute(conout, 0x42, 1, writeCoord, &actual);
    assert(ret && actual == 1);

    countDown(5);
}
