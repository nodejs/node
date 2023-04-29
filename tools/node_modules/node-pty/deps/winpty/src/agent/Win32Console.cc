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

#include "Win32Console.h"

#include <windows.h>
#include <wchar.h>

#include <string>

#include "../shared/DebugClient.h"
#include "../shared/WinptyAssert.h"

Win32Console::Win32Console() : m_titleWorkBuf(16)
{
    // The console window must be non-NULL.  It is used for two purposes:
    //  (1) "Freezing" the console to detect the exact number of lines that
    //      have scrolled.
    //  (2) Killing processes attached to the console, by posting a WM_CLOSE
    //      message to the console window.
    m_hwnd = GetConsoleWindow();
    ASSERT(m_hwnd != nullptr);
}

std::wstring Win32Console::title()
{
    while (true) {
        // Calling GetConsoleTitleW is tricky, because its behavior changed
        // from XP->Vista, then again from Win7->Win8.  The Vista+Win7 behavior
        // is especially broken.
        //
        // The MSDN documentation documents nSize as the "size of the buffer
        // pointed to by the lpConsoleTitle parameter, in characters" and the
        // successful return value as "the length of the console window's
        // title, in characters."
        //
        // On XP, the function returns the title length, AFTER truncation
        // (excluding the NUL terminator).  If the title is blank, the API
        // returns 0 and does not NUL-terminate the buffer.  To accommodate
        // XP, the function must:
        //  * Terminate the buffer itself.
        //  * Double the size of the title buffer in a loop.
        //
        // On Vista and up, the function returns the non-truncated title
        // length (excluding the NUL terminator).
        //
        // On Vista and Windows 7, there is a bug where the buffer size is
        // interpreted as a byte count rather than a wchar_t count.  To
        // work around this, we must pass GetConsoleTitleW a buffer that is
        // twice as large as what is actually needed.
        //
        // See misc/*/Test_GetConsoleTitleW.cc for tests demonstrating Windows'
        // behavior.

        DWORD count = GetConsoleTitleW(m_titleWorkBuf.data(),
                                       m_titleWorkBuf.size());
        const size_t needed = (count + 1) * sizeof(wchar_t);
        if (m_titleWorkBuf.size() < needed) {
            m_titleWorkBuf.resize(needed);
            continue;
        }
        m_titleWorkBuf[count] = L'\0';
        return m_titleWorkBuf.data();
    }
}

void Win32Console::setTitle(const std::wstring &title)
{
    if (!SetConsoleTitleW(title.c_str())) {
        trace("SetConsoleTitleW failed");
    }
}

void Win32Console::setFrozen(bool frozen) {
    const int SC_CONSOLE_MARK = 0xFFF2;
    const int SC_CONSOLE_SELECT_ALL = 0xFFF5;
    if (frozen == m_frozen) {
        // Do nothing.
    } else if (frozen) {
        // Enter selection mode by activating either Mark or SelectAll.
        const int command = m_freezeUsesMark ? SC_CONSOLE_MARK
                                             : SC_CONSOLE_SELECT_ALL;
        SendMessage(m_hwnd, WM_SYSCOMMAND, command, 0);
        m_frozen = true;
    } else {
        // Send Escape to cancel the selection.
        SendMessage(m_hwnd, WM_CHAR, 27, 0x00010001);
        m_frozen = false;
    }
}
