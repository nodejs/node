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

#include "StringUtil.h"

#include <windows.h>

#include "WinptyAssert.h"

// Workaround.  MinGW (from mingw.org) does not have wcsnlen.  MinGW-w64 *does*
// have wcsnlen, but use this function for consistency.
size_t winpty_wcsnlen(const wchar_t *s, size_t maxlen) {
    ASSERT(s != NULL);
    for (size_t i = 0; i < maxlen; ++i) {
        if (s[i] == L'\0') {
            return i;
        }
    }
    return maxlen;
}

std::string utf8FromWide(const std::wstring &input) {
    int mblen = WideCharToMultiByte(
        CP_UTF8, 0,
        input.data(), input.size(),
        NULL, 0, NULL, NULL);
    if (mblen <= 0) {
        return std::string();
    }
    std::vector<char> tmp(mblen);
    int mblen2 = WideCharToMultiByte(
        CP_UTF8, 0,
        input.data(), input.size(),
        tmp.data(), tmp.size(),
        NULL, NULL);
    ASSERT(mblen2 == mblen);
    return std::string(tmp.data(), tmp.size());
}
