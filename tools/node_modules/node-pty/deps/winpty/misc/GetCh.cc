#include <conio.h>
#include <ctype.h>
#include <stdio.h>

int main() {
    printf("\nPress any keys -- Ctrl-D exits\n\n");

    while (true) {
        const int ch = getch();
        printf("0x%x", ch);
        if (isgraph(ch)) {
            printf(" '%c'", ch);
        }
        printf("\n");
        if (ch == 0x4) { // Ctrl-D
            break;
        }
    }
    return 0;
}
