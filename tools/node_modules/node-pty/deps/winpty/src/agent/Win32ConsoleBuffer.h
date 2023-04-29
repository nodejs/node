// Copyright (c) 2011-2016 Ryan Prichard
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

#ifndef AGENT_WIN32_CONSOLE_BUFFER_H
#define AGENT_WIN32_CONSOLE_BUFFER_H

#include <windows.h>

#include <string.h>

#include <memory>

#include "Coord.h"
#include "SmallRect.h"

class ConsoleScreenBufferInfo : public CONSOLE_SCREEN_BUFFER_INFO {
public:
    ConsoleScreenBufferInfo()
    {
        memset(this, 0, sizeof(*this));
    }

    Coord bufferSize() const        { return dwSize;    }
    SmallRect windowRect() const    { return srWindow;  }
    Coord cursorPosition() const    { return dwCursorPosition; }
};

class Win32ConsoleBuffer {
private:
    Win32ConsoleBuffer(HANDLE conout, bool owned) :
        m_conout(conout), m_owned(owned)
    {
    }

public:
    static const int kDefaultAttributes = 7;

    ~Win32ConsoleBuffer() {
        if (m_owned) {
            CloseHandle(m_conout);
        }
    }

    static std::unique_ptr<Win32ConsoleBuffer> openStdout();
    static std::unique_ptr<Win32ConsoleBuffer> openConout();
    static std::unique_ptr<Win32ConsoleBuffer> createErrorBuffer();

    Win32ConsoleBuffer(const Win32ConsoleBuffer &other) = delete;
    Win32ConsoleBuffer &operator=(const Win32ConsoleBuffer &other) = delete;

    HANDLE conout();
    void clearLines(int row, int count, const ConsoleScreenBufferInfo &info);
    void clearAllLines(const ConsoleScreenBufferInfo &info);

    // Buffer and window sizes.
    ConsoleScreenBufferInfo bufferInfo();
    Coord bufferSize();
    SmallRect windowRect();
    void resizeBuffer(const Coord &size);
    bool resizeBufferRange(const Coord &initialSize, Coord &finalSize);
    bool resizeBufferRange(const Coord &initialSize) {
        Coord dummy;
        return resizeBufferRange(initialSize, dummy);
    }
    void moveWindow(const SmallRect &rect);

    // Cursor.
    Coord cursorPosition();
    void setCursorPosition(const Coord &point);

    // Screen content.
    void read(const SmallRect &rect, CHAR_INFO *data);
    void write(const SmallRect &rect, const CHAR_INFO *data);

    void setTextAttribute(WORD attributes);

private:
    HANDLE m_conout = nullptr;
    bool m_owned = false;
};

#endif // AGENT_WIN32_CONSOLE_BUFFER_H
