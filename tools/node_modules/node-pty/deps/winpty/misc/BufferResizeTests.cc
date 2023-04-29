#include <windows.h>
#include <cassert>

#include "TestUtil.cc"

void dumpInfoToTrace() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    assert(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info));
    trace("win=(%d,%d,%d,%d)",
        (int)info.srWindow.Left,
        (int)info.srWindow.Top,
        (int)info.srWindow.Right,
        (int)info.srWindow.Bottom);
    trace("buf=(%d,%d)",
        (int)info.dwSize.X,
        (int)info.dwSize.Y);
    trace("cur=(%d,%d)",
        (int)info.dwCursorPosition.X,
        (int)info.dwCursorPosition.Y);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    setWindowPos(0, 0, 1, 1);

    if (false) {
        // Reducing the buffer height can move the window up.
        setBufferSize(80, 25);
        setWindowPos(0, 20, 80, 5);
        Sleep(2000);
        setBufferSize(80, 10);
    }

    if (false) {
        // Reducing the buffer height moves the window up and the buffer
        // contents up too.
        setBufferSize(80, 25);
        setWindowPos(0, 20, 80, 5);
        setCursorPos(0, 20);
        printf("TEST1\nTEST2\nTEST3\nTEST4\n");
        fflush(stdout);
        Sleep(2000);
        setBufferSize(80, 10);
    }

    if (false) {
        // Reducing the buffer width can move the window left.
        setBufferSize(80, 25);
        setWindowPos(40, 0, 40, 25);
        Sleep(2000);
        setBufferSize(60, 25);
    }

    if (false) {
        // Sometimes the buffer contents are shifted up; sometimes they're
        // shifted down.  It seems to depend on the cursor position?

        // setBufferSize(80, 25);
        // setWindowPos(0, 20, 80, 5);
        // setCursorPos(0, 20);
        // printf("TESTa\nTESTb\nTESTc\nTESTd\nTESTe");
        // fflush(stdout);
        // setCursorPos(0, 0);
        // printf("TEST1\nTEST2\nTEST3\nTEST4\nTEST5");
        // fflush(stdout);
        // setCursorPos(0, 24);
        // Sleep(5000);
        // setBufferSize(80, 24);

        setBufferSize(80, 20);
        setWindowPos(0, 10, 80, 10);
        setCursorPos(0, 18);

        printf("TEST1\nTEST2");
        fflush(stdout);
        setCursorPos(0, 18);

        Sleep(2000);
        setBufferSize(80, 18);
    }

    dumpInfoToTrace();
    Sleep(30000);

    return 0;
}
