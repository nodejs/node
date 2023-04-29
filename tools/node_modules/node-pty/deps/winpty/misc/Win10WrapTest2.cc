#include <windows.h>

#include "TestUtil.cc"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    const int WIDTH = 25;

    setWindowPos(0, 0, 1, 1);
    setBufferSize(WIDTH, 40);
    setWindowPos(0, 0, WIDTH, 20);

    system("cls");

    for (int i = 0; i < 100; ++i) {
        printf("FOO(%d)\n", i);
    }

    repeatChar(5, '\n');
    repeatChar(WIDTH * 5, '.');
    repeatChar(10, '\n');
    setWindowPos(0, 20, WIDTH, 20);
    writeBox(0, 5, 1, 10, '|');

    Sleep(120000);
}
