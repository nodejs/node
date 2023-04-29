// Determines whether this is a new console by testing whether MARK moves the
// cursor.
//
// WARNING: This test program may behave erratically if run under winpty.
//

#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "TestUtil.cc"

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;

static COORD getWindowPos(HANDLE conout) {
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    BOOL ret = GetConsoleScreenBufferInfo(conout, &info);
    ASSERT(ret && "GetConsoleScreenBufferInfo failed");
    return { info.srWindow.Left, info.srWindow.Top };
}

static COORD getWindowSize(HANDLE conout) {
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    BOOL ret = GetConsoleScreenBufferInfo(conout, &info);
    ASSERT(ret && "GetConsoleScreenBufferInfo failed");
    return {
        static_cast<short>(info.srWindow.Right - info.srWindow.Left + 1),
        static_cast<short>(info.srWindow.Bottom - info.srWindow.Top + 1)
    };
}

static COORD getCursorPos(HANDLE conout) {
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    BOOL ret = GetConsoleScreenBufferInfo(conout, &info);
    ASSERT(ret && "GetConsoleScreenBufferInfo failed");
    return info.dwCursorPosition;
}

static void setCursorPos(HANDLE conout, COORD pos) {
    BOOL ret = SetConsoleCursorPosition(conout, pos);
    ASSERT(ret && "SetConsoleCursorPosition failed");
}

int main() {
    const HANDLE conout = openConout();
    const HWND hwnd = GetConsoleWindow();
    ASSERT(hwnd != NULL && "GetConsoleWindow() returned NULL");

    // With the legacy console, the Mark command moves the the cursor to the
    // top-left cell of the visible console window.  Determine whether this
    // is the new console by seeing if the cursor moves.

    const auto windowSize = getWindowSize(conout);
    if (windowSize.X <= 1) {
        printf("Error: console window must be at least 2 columns wide\n");
        trace("Error: console window must be at least 2 columns wide");
        return 1;
    }

    bool cursorMoved = false;
    const auto initialPos = getCursorPos(conout);

    const auto windowPos = getWindowPos(conout);
    setCursorPos(conout, { static_cast<short>(windowPos.X + 1), windowPos.Y });

    {
        const auto posA = getCursorPos(conout);
        SendMessage(hwnd, WM_SYSCOMMAND, SC_CONSOLE_MARK, 0);
        const auto posB = getCursorPos(conout);
        cursorMoved = memcmp(&posA, &posB, sizeof(posA)) != 0;
        SendMessage(hwnd, WM_CHAR, 27, 0x00010001); // Send ESCAPE
    }

    setCursorPos(conout, initialPos);

    if (cursorMoved) {
        printf("Legacy console (i.e. MARK moved cursor)\n");
        trace("Legacy console (i.e. MARK moved cursor)");
    } else {
        printf("Windows 10 new console (i.e MARK did not move cursor)\n");
        trace("Windows 10 new console (i.e MARK did not move cursor)");
    }

    return 0;
}
