// Copyright (c) 2016 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef WINPTY_SNPRINTF_H
#define WINPTY_SNPRINTF_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "WinptyAssert.h"

#if defined(__CYGWIN__) || defined(__MSYS__)
#define WINPTY_SNPRINTF_FORMAT(fmtarg, vararg) \
    __attribute__((format(printf, (fmtarg), ((vararg)))))
#elif defined(__GNUC__)
#define WINPTY_SNPRINTF_FORMAT(fmtarg, vararg) \
    __attribute__((format(ms_printf, (fmtarg), ((vararg)))))
#else
#define WINPTY_SNPRINTF_FORMAT(fmtarg, vararg)
#endif

// Returns a value between 0 and size - 1 (inclusive) on success.  Returns -1
// on failure (including truncation).  The output buffer is always
// NUL-terminated.
inline int
winpty_vsnprintf(char *out, size_t size, const char *fmt, va_list ap) {
    ASSERT(size > 0);
    out[0] = '\0';
#if defined(_MSC_VER) && _MSC_VER < 1900
    // MSVC 2015 added a C99-conforming vsnprintf.
    int count = _vsnprintf_s(out, size, _TRUNCATE, fmt, ap);
#else
    // MinGW configurations frequently provide a vsnprintf function that simply
    // calls one of the MS _vsnprintf* functions, which are not C99 conformant.
    int count = vsnprintf(out, size, fmt, ap);
#endif
    if (count < 0 || static_cast<size_t>(count) >= size) {
        // On truncation, some *printf* implementations return the
        // non-truncated size, but other implementations returns -1.  Return
        // -1 for consistency.
        count = -1;
        // Guarantee NUL termination.
        out[size - 1] = '\0';
    } else {
        // Guarantee NUL termination.
        out[count] = '\0';
    }
    return count;
}

// Wraps winpty_vsnprintf.
inline int winpty_snprintf(char *out, size_t size, const char *fmt, ...)
    WINPTY_SNPRINTF_FORMAT(3, 4);
inline int winpty_snprintf(char *out, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int count = winpty_vsnprintf(out, size, fmt, ap);
    va_end(ap);
    return count;
}

// Wraps winpty_vsnprintf with automatic size determination.
template <size_t size>
int winpty_vsnprintf(char (&out)[size], const char *fmt, va_list ap) {
    return winpty_vsnprintf(out, size, fmt, ap);
}

// Wraps winpty_vsnprintf with automatic size determination.
template <size_t size>
int winpty_snprintf(char (&out)[size], const char *fmt, ...)
    WINPTY_SNPRINTF_FORMAT(2, 3);
template <size_t size>
int winpty_snprintf(char (&out)[size], const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int count = winpty_vsnprintf(out, size, fmt, ap);
    va_end(ap);
    return count;
}

#endif // WINPTY_SNPRINTF_H
