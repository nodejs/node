#include <ctype.h>
#include <stdio.h>
#include <string.h>

static inline void formatChar(char *str, char ch)
{
    // Print some common control codes.
    switch (ch) {
    case '\r': strcpy(str, "CR "); break;
    case '\n': strcpy(str, "LF "); break;
    case ' ':  strcpy(str, "SP "); break;
    case 27:   strcpy(str, "^[ "); break;
    case 3:    strcpy(str, "^C "); break;
    default:
        if (isgraph(ch))
            sprintf(str, "%c ", ch);
        else
            sprintf(str, "%02x ", ch);
        break;
    }
}
