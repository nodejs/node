#include <windows.h>

#include "TestUtil.cc"

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 5) {
        printf("Usage: %s x y\n", argv[0]);
        printf("Usage: %s x y width height\n", argv[0]);
        return 1;
    }

    HWND hwnd = GetConsoleWindow();

    const int x = atoi(argv[1]);
    const int y = atoi(argv[2]);

    int w = 0, h = 0;
    if (argc == 3) {
        RECT r = {};
        BOOL ret = GetWindowRect(hwnd, &r);
        ASSERT(ret && "GetWindowRect failed on console window");
        w = r.right - r.left;
        h = r.bottom - r.top;
    } else {
        w = atoi(argv[3]);
        h = atoi(argv[4]);
    }

    BOOL ret = MoveWindow(hwnd, x, y, w, h, TRUE);
    trace("MoveWindow: ret=%d", ret);
    printf("MoveWindow: ret=%d\n", ret);

    return 0;
}
