/*
 * A Win32 program that reads raw console input with getch and echos
 * it to stdout.
 */

#include <stdio.h>
#include <conio.h>

int main()
{
    int count = 0;
    while (true) {
        int ch = getch();
        printf("%02x ", ch);
        if (++count == 50)
            break;
    }
    return 0;
}
