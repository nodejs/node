// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *aprintf(char **s, const char *fmt, ...) {
    char *c = NULL;
    int n, len;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap) + 1;
    va_end(ap);

    len = *s ? strlen(*s) : 0;

    if ((*s = realloc(*s, (len + n) * sizeof(char)))) {
        c = *s + len;
        va_start(ap, fmt);
        vsnprintf(c, n, fmt, ap);
        va_end(ap);
    }

    return c;
}
