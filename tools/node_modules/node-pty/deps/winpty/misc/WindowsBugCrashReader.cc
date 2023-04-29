// I noticed this on the ConEmu web site:
//
// https://social.msdn.microsoft.com/Forums/en-US/40c8e395-cca9-45c8-b9b8-2fbe6782ac2b/readconsoleoutput-cause-access-violation-writing-location-exception
// https://conemu.github.io/en/MicrosoftBugs.html
//
// In Windows 7, 8, and 8.1, a ReadConsoleOutputW with an out-of-bounds read
// region crashes the application.  I have reproduced the problem on Windows 8
// and 8.1, but not on Windows 7.
//

#include <windows.h>

#include "TestUtil.cc"

int main() {
    setWindowPos(0, 0, 1, 1);
    setBufferSize(80, 25);
    setWindowPos(0, 0, 80, 25);

    const HANDLE conout = openConout();
    static CHAR_INFO lineBuf[80];
    SMALL_RECT readRegion = { 0, 999, 79, 999 };
    const BOOL ret = ReadConsoleOutputW(conout, lineBuf, {80, 1}, {0, 0}, &readRegion);
    ASSERT(!ret && "ReadConsoleOutputW should have failed");

    return 0;
}
