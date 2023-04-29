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

#include "Win32ConsoleBuffer.h"

#include <windows.h>

#include "../shared/DebugClient.h"
#include "../shared/StringBuilder.h"
#include "../shared/WinptyAssert.h"

std::unique_ptr<Win32ConsoleBuffer> Win32ConsoleBuffer::openStdout() {
    return std::unique_ptr<Win32ConsoleBuffer>(
        new Win32ConsoleBuffer(GetStdHandle(STD_OUTPUT_HANDLE), false));
}

std::unique_ptr<Win32ConsoleBuffer> Win32ConsoleBuffer::openConout() {
    const HANDLE conout = CreateFileW(L"CONOUT$",
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(conout != INVALID_HANDLE_VALUE);
    return std::unique_ptr<Win32ConsoleBuffer>(
        new Win32ConsoleBuffer(conout, true));
}

std::unique_ptr<Win32ConsoleBuffer> Win32ConsoleBuffer::createErrorBuffer() {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    const HANDLE conout =
        CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &sa,
                                  CONSOLE_TEXTMODE_BUFFER,
                                  nullptr);
    ASSERT(conout != INVALID_HANDLE_VALUE);
    return std::unique_ptr<Win32ConsoleBuffer>(
        new Win32ConsoleBuffer(conout, true));
}

HANDLE Win32ConsoleBuffer::conout() {
    return m_conout;
}

void Win32ConsoleBuffer::clearLines(
        int row,
        int count,
        const ConsoleScreenBufferInfo &info) {
    // TODO: error handling
    const int width = info.bufferSize().X;
    DWORD actual = 0;
    if (!FillConsoleOutputCharacterW(
            m_conout, L' ', width * count, Coord(0, row),
            &actual) || static_cast<int>(actual) != width * count) {
        trace("FillConsoleOutputCharacterW failed");
    }
    if (!FillConsoleOutputAttribute(
            m_conout, kDefaultAttributes, width * count, Coord(0, row),
            &actual) || static_cast<int>(actual) != width * count) {
        trace("FillConsoleOutputAttribute failed");
    }
}

void Win32ConsoleBuffer::clearAllLines(const ConsoleScreenBufferInfo &info) {
    clearLines(0, info.bufferSize().Y, info);
}

ConsoleScreenBufferInfo Win32ConsoleBuffer::bufferInfo() {
    // TODO: error handling
    ConsoleScreenBufferInfo info;
    if (!GetConsoleScreenBufferInfo(m_conout, &info)) {
        trace("GetConsoleScreenBufferInfo failed");
    }
    return info;
}

Coord Win32ConsoleBuffer::bufferSize() {
    return bufferInfo().bufferSize();
}

SmallRect Win32ConsoleBuffer::windowRect() {
    return bufferInfo().windowRect();
}

bool Win32ConsoleBuffer::resizeBufferRange(const Coord &initialSize,
                                           Coord &finalSize) {
    if (SetConsoleScreenBufferSize(m_conout, initialSize)) {
        finalSize = initialSize;
        return true;
    }
    // The font might be too small to accommodate a very narrow console window.
    // In that case, rather than simply give up, it's better to try wider
    // buffer sizes until the call succeeds.
    Coord size = initialSize;
    while (size.X < 20) {
        size.X++;
        if (SetConsoleScreenBufferSize(m_conout, size)) {
            finalSize = size;
            trace("SetConsoleScreenBufferSize: initial size (%d,%d) failed, "
                  "but wider size (%d,%d) succeeded",
                  initialSize.X, initialSize.Y,
                  finalSize.X, finalSize.Y);
            return true;
        }
    }
    trace("SetConsoleScreenBufferSize failed: "
          "tried (%d,%d) through (%d,%d)",
          initialSize.X, initialSize.Y,
          size.X, size.Y);
    return false;
}

void Win32ConsoleBuffer::resizeBuffer(const Coord &size) {
    // TODO: error handling
    if (!SetConsoleScreenBufferSize(m_conout, size)) {
        trace("SetConsoleScreenBufferSize failed: size=(%d,%d)",
              size.X, size.Y);
    }
}

void Win32ConsoleBuffer::moveWindow(const SmallRect &rect) {
    // TODO: error handling
    if (!SetConsoleWindowInfo(m_conout, TRUE, &rect)) {
        trace("SetConsoleWindowInfo failed");
    }
}

Coord Win32ConsoleBuffer::cursorPosition() {
    return bufferInfo().dwCursorPosition;
}

void Win32ConsoleBuffer::setCursorPosition(const Coord &coord) {
    // TODO: error handling
    if (!SetConsoleCursorPosition(m_conout, coord)) {
        trace("SetConsoleCursorPosition failed");
    }
}

void Win32ConsoleBuffer::read(const SmallRect &rect, CHAR_INFO *data) {
    // TODO: error handling
    SmallRect tmp(rect);
    if (!ReadConsoleOutputW(m_conout, data, rect.size(), Coord(), &tmp) &&
            isTracingEnabled()) {
        StringBuilder sb(256);
        auto outStruct = [&](const SMALL_RECT &sr) {
            sb << "{L=" << sr.Left << ",T=" << sr.Top
               << ",R=" << sr.Right << ",B=" << sr.Bottom << '}';
        };
        sb << "Win32ConsoleBuffer::read: ReadConsoleOutput failed: readRegion=";
        outStruct(rect);
        CONSOLE_SCREEN_BUFFER_INFO info = {};
        if (GetConsoleScreenBufferInfo(m_conout, &info)) {
            sb << ", dwSize=(" << info.dwSize.X << ',' << info.dwSize.Y
               << "), srWindow=";
            outStruct(info.srWindow);
        } else {
            sb << ", GetConsoleScreenBufferInfo also failed";
        }
        trace("%s", sb.c_str());
    }
}

void Win32ConsoleBuffer::write(const SmallRect &rect, const CHAR_INFO *data) {
    // TODO: error handling
    SmallRect tmp(rect);
    if (!WriteConsoleOutputW(m_conout, data, rect.size(), Coord(), &tmp)) {
        trace("WriteConsoleOutput failed");
    }
}

void Win32ConsoleBuffer::setTextAttribute(WORD attributes) {
    if (!SetConsoleTextAttribute(m_conout, attributes)) {
        trace("SetConsoleTextAttribute failed");
    }
}
