#include <windows.h>
#include <stdio.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
    static int escCount = 0;

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    while (true) {
        DWORD count;
        INPUT_RECORD ir;
        if (!ReadConsoleInput(hStdin, &ir, 1, &count)) {
            printf("ReadConsoleInput failed\n");
            return 1;
        }

        if (true) {
            DWORD mode;
            GetConsoleMode(hStdin, &mode);
            SetConsoleMode(hStdin, mode & ~ENABLE_PROCESSED_INPUT);
        }

        if (ir.EventType == KEY_EVENT) {
            const KEY_EVENT_RECORD &ker = ir.Event.KeyEvent;
            printf("%s", ker.bKeyDown ? "dn" : "up");
            printf(" ch=");
            if (isprint(ker.uChar.AsciiChar))
                printf("'%c'", ker.uChar.AsciiChar);
            printf("%d", ker.uChar.AsciiChar);
            printf(" vk=%#x", ker.wVirtualKeyCode);
            printf(" scan=%#x", ker.wVirtualScanCode);
            printf(" state=%#x", (int)ker.dwControlKeyState);
            printf(" repeat=%d", ker.wRepeatCount);
            printf("\n");
            if (ker.uChar.AsciiChar == 27 && ++escCount == 6)
                break;
        }
    }
}
