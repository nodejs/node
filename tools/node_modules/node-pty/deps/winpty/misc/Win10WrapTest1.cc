/*
 * Demonstrates some wrapping behaviors of the new Windows 10 console.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "TestUtil.cc"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    setWindowPos(0, 0, 1, 1);
    setBufferSize(40, 20);
    setWindowPos(0, 0, 40, 20);

    system("cls");

    repeatChar(39, 'A'); repeatChar(1, ' ');
    repeatChar(39, 'B'); repeatChar(1, ' ');
    printf("\n");

    repeatChar(39, 'C'); repeatChar(1, ' ');
    repeatChar(39, 'D'); repeatChar(1, ' ');
    printf("\n");

    repeatChar(40, 'E');
    repeatChar(40, 'F');
    printf("\n");

    repeatChar(39, 'G'); repeatChar(1, ' ');
    repeatChar(39, 'H'); repeatChar(1, ' ');
    printf("\n");

    Sleep(2000);

    setChar(39, 0, '*', 0x24);
    setChar(39, 1, '*', 0x24);

    setChar(39, 3, ' ', 0x24);
    setChar(39, 4, ' ', 0x24);

    setChar(38, 6, ' ', 0x24);
    setChar(38, 7, ' ', 0x24);

    Sleep(2000);
    setWindowPos(0, 0, 35, 20);
    setBufferSize(35, 20);
    trace("DONE");

    printf("Sleeping forever...\n");
    while(true) { Sleep(1000); }
}
