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

#ifndef LARGE_CONSOLE_READ_H
#define LARGE_CONSOLE_READ_H

#include <windows.h>
#include <stdlib.h>

#include <vector>

#include "SmallRect.h"
#include "../shared/DebugClient.h"
#include "../shared/WinptyAssert.h"

class Win32ConsoleBuffer;

class LargeConsoleReadBuffer {
public:
    LargeConsoleReadBuffer();
    const SmallRect &rect() const { return m_rect; }
    const CHAR_INFO *lineData(int line) const {
        validateLineNumber(line);
        return &m_data[(line - m_rect.Top) * m_rectWidth];
    }

private:
    CHAR_INFO *lineDataMut(int line) {
        validateLineNumber(line);
        return &m_data[(line - m_rect.Top) * m_rectWidth];
    }

    void validateLineNumber(int line) const {
        if (line < m_rect.Top || line > m_rect.Bottom) {
            trace("Fatal error: LargeConsoleReadBuffer: invalid line %d for "
                  "read rect %s", line, m_rect.toString().c_str());
            abort();
        }
    }

    SmallRect m_rect;
    int m_rectWidth;
    std::vector<CHAR_INFO> m_data;

    friend void largeConsoleRead(LargeConsoleReadBuffer &out,
                                 Win32ConsoleBuffer &buffer,
                                 const SmallRect &readArea,
                                 WORD attributesMask);
};

#endif // LARGE_CONSOLE_READ_H
