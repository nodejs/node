#include <windows.h>

#include "TestUtil.cc"

const int SC_CONSOLE_MARK = 0xFFF2;
const int SC_CONSOLE_SELECT_ALL = 0xFFF5;

int main(int argc, char *argv[0]) {

    if (argc != 2) {
        printf("Usage: %s (mark|selectall|read)\n", argv[0]);
        return 1;
    }

    enum class Test { Mark, SelectAll, Read } test;
    if (!strcmp(argv[1], "mark")) {
        test = Test::Mark;
    } else if (!strcmp(argv[1], "selectall")) {
        test = Test::SelectAll;
    } else if (!strcmp(argv[1], "read")) {
        test = Test::Read;
    } else {
        printf("Invalid test: %s\n", argv[1]);
        return 1;
    }

    HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    TimeMeasurement tm;
    HWND hwnd = GetConsoleWindow();

    setWindowPos(0, 0, 1, 1);
    setBufferSize(100, 3000);
    system("cls");
    setWindowPos(0, 2975, 100, 25);
    setCursorPos(0, 2999);

    ShowWindow(hwnd, SW_HIDE);

    for (int i = 0; i < 1000; ++i) {
        // CONSOLE_SCREEN_BUFFER_INFO info = {};
        // GetConsoleScreenBufferInfo(conout, &info);

        if (test == Test::Mark) {
            SendMessage(hwnd, WM_SYSCOMMAND, SC_CONSOLE_MARK, 0);
            SendMessage(hwnd, WM_CHAR, 27, 0x00010001);
        } else if (test == Test::SelectAll) {
            SendMessage(hwnd, WM_SYSCOMMAND, SC_CONSOLE_SELECT_ALL, 0);
            SendMessage(hwnd, WM_CHAR, 27, 0x00010001);
        } else if (test == Test::Read) {
            static CHAR_INFO buffer[100 * 3000];
            const SMALL_RECT readRegion = {0, 0, 99, 2999};
            SMALL_RECT tmp = readRegion;
            BOOL ret = ReadConsoleOutput(conout, buffer, {100, 3000}, {0, 0}, &tmp);
            ASSERT(ret && !memcmp(&tmp, &readRegion, sizeof(tmp)));
        }
    }

    ShowWindow(hwnd, SW_SHOW);

    printf("elapsed: %f\n", tm.elapsed());
    return 0;
}
