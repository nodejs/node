#include <windows.h>

#include <stdio.h>

#include "TestUtil.cc"

int main() {
    const HANDLE conout = openConout();

    CONSOLE_SCREEN_BUFFER_INFO info = {};
    BOOL ret = GetConsoleScreenBufferInfo(conout, &info);
    ASSERT(ret && "GetConsoleScreenBufferInfo failed");

    trace("cursor=%d,%d", info.dwCursorPosition.X, info.dwCursorPosition.Y);
    printf("cursor=%d,%d\n", info.dwCursorPosition.X, info.dwCursorPosition.Y);

    trace("srWindow={L=%d,T=%d,R=%d,B=%d}", info.srWindow.Left, info.srWindow.Top, info.srWindow.Right, info.srWindow.Bottom);
    printf("srWindow={L=%d,T=%d,R=%d,B=%d}\n", info.srWindow.Left, info.srWindow.Top, info.srWindow.Right, info.srWindow.Bottom);

    trace("dwSize=%d,%d", info.dwSize.X, info.dwSize.Y);
    printf("dwSize=%d,%d\n", info.dwSize.X, info.dwSize.Y);

    const HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        RECT r = {};
        if (GetWindowRect(hwnd, &r)) {
            const int w = r.right - r.left;
            const int h = r.bottom - r.top;
            trace("hwnd: pos=(%d,%d) size=(%d,%d)", r.left, r.top, w, h);
            printf("hwnd: pos=(%d,%d) size=(%d,%d)\n", r.left, r.top, w, h);
        } else {
            trace("GetWindowRect failed");
            printf("GetWindowRect failed\n");
        }
    } else {
        trace("GetConsoleWindow returned NULL");
        printf("GetConsoleWindow returned NULL\n");
    }

    return 0;
}
