// Copyright (c) 2015 Ryan Prichard
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

#ifndef WINPTY_SHARED_STRING_UTIL_H
#define WINPTY_SHARED_STRING_UTIL_H

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <algorithm>
#include <string>
#include <vector>

#include "WinptyAssert.h"

size_t winpty_wcsnlen(const wchar_t *s, size_t maxlen);
std::string utf8FromWide(const std::wstring &input);

// Return a vector containing each character in the string.
template <typename T>
std::vector<T> vectorFromString(const std::basic_string<T> &str) {
    return std::vector<T>(str.begin(), str.end());
}

// Return a vector containing each character in the string, followed by a
// NUL terminator.
template <typename T>
std::vector<T> vectorWithNulFromString(const std::basic_string<T> &str) {
    std::vector<T> ret;
    ret.reserve(str.size() + 1);
    ret.insert(ret.begin(), str.begin(), str.end());
    ret.push_back('\0');
    return ret;
}

// A safer(?) version of wcsncpy that is accepted by MSVC's /SDL mode.
template <size_t N>
wchar_t *winpty_wcsncpy(wchar_t (&d)[N], const wchar_t *s) {
    ASSERT(s != nullptr);
    size_t i = 0;
    for (; i < N; ++i) {
        if (s[i] == L'\0') {
            break;
        }
        d[i] = s[i];
    }
    for (; i < N; ++i) {
        d[i] = L'\0';
    }
    return d;
}

// Like wcsncpy, but ensure that the destination buffer is NUL-terminated.
template <size_t N>
wchar_t *winpty_wcsncpy_nul(wchar_t (&d)[N], const wchar_t *s) {
    static_assert(N > 0, "array cannot be 0-size");
    winpty_wcsncpy(d, s);
    d[N - 1] = L'\0';
    return d;
}

#endif // WINPTY_SHARED_STRING_UTIL_H
