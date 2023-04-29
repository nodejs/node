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

#ifndef CONSOLEINPUT_H
#define CONSOLEINPUT_H

#include <windows.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "Coord.h"
#include "InputMap.h"
#include "SmallRect.h"

class Win32Console;
class DsrSender;

class ConsoleInput
{
public:
    ConsoleInput(HANDLE conin, int mouseMode, DsrSender &dsrSender,
                 Win32Console &console);
    void writeInput(const std::string &input);
    void flushIncompleteEscapeCode();
    void setMouseWindowRect(SmallRect val) { m_mouseWindowRect = val; }
    void updateInputFlags(bool forceTrace=false);
    bool shouldActivateTerminalMouse();

private:
    void doWrite(bool isEof);
    void flushInputRecords(std::vector<INPUT_RECORD> &records);
    int scanInput(std::vector<INPUT_RECORD> &records,
                  const char *input,
                  int inputSize,
                  bool isEof);
    int scanMouseInput(std::vector<INPUT_RECORD> &records,
                       const char *input,
                       int inputSize);
    void appendUtf8Char(std::vector<INPUT_RECORD> &records,
                        const char *charBuffer,
                        int charLen,
                        bool terminalAltEscape);
    void appendKeyPress(std::vector<INPUT_RECORD> &records,
                        uint16_t virtualKey,
                        uint32_t winCodePointDn,
                        uint32_t winCodePointUp,
                        uint16_t winKeyState,
                        uint32_t vtCodePoint,
                        uint16_t vtKeyState);

public:
    static void appendCPInputRecords(std::vector<INPUT_RECORD> &records,
                                     BOOL keyDown,
                                     uint16_t virtualKey,
                                     uint32_t codePoint,
                                     uint16_t keyState);
    static void appendInputRecord(std::vector<INPUT_RECORD> &records,
                                  BOOL keyDown,
                                  uint16_t virtualKey,
                                  wchar_t utf16Char,
                                  uint16_t keyState);

private:
    DWORD inputConsoleMode();

private:
    Win32Console &m_console;
    HANDLE m_conin = nullptr;
    int m_mouseMode = 0;
    DsrSender &m_dsrSender;
    bool m_dsrSent = false;
    std::string m_byteQueue;
    InputMap m_inputMap;
    DWORD m_lastWriteTick = 0;
    DWORD m_mouseButtonState = 0;
    struct DoubleClickDetection {
        DWORD button = 0;
        Coord pos;
        DWORD tick = 0;
        bool released = false;
    } m_doubleClick;
    bool m_enableExtendedEnabled = false;
    bool m_mouseInputEnabled = false;
    bool m_quickEditEnabled = false;
    bool m_escapeInputEnabled = false;
    SmallRect m_mouseWindowRect;
};

#endif // CONSOLEINPUT_H
