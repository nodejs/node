// Copyright (c) 2011-2015 Ryan Prichard
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

#ifndef TERMINAL_H
#define TERMINAL_H

#include <windows.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Coord.h"

class NamedPipe;

class Terminal
{
public:
    explicit Terminal(NamedPipe &output, bool plainMode, bool outputColor)
        : m_output(output), m_plainMode(plainMode), m_outputColor(outputColor)
    {
    }

    enum SendClearFlag { OmitClear, SendClear };
    void reset(SendClearFlag sendClearFirst, int64_t newLine);
    void sendLine(int64_t line, const CHAR_INFO *lineData, int width,
                  int cursorColumn);
    void showTerminalCursor(int column, int64_t line);
    void hideTerminalCursor();

private:
    void moveTerminalToLine(int64_t line);

public:
    void enableMouseMode(bool enabled);

private:
    NamedPipe &m_output;
    int64_t m_remoteLine = 0;
    int m_remoteColumn = 0;
    bool m_lineDataValid = true;
    std::vector<CHAR_INFO> m_lineData;
    bool m_cursorHidden = false;
    int m_remoteColor = -1;
    std::string m_termLineWorkingBuffer;
    bool m_plainMode = false;
    bool m_outputColor = true;
    bool m_mouseModeEnabled = false;
};

#endif // TERMINAL_H
