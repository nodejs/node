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

#ifndef AGENT_WIN32_CONSOLE_H
#define AGENT_WIN32_CONSOLE_H

#include <windows.h>

#include <string>
#include <vector>

class Win32Console
{
public:
    class FreezeGuard {
    public:
        FreezeGuard(Win32Console &console, bool frozen) :
                m_console(console), m_previous(console.frozen()) {
            m_console.setFrozen(frozen);
        }
        ~FreezeGuard() {
            m_console.setFrozen(m_previous);
        }
        FreezeGuard(const FreezeGuard &other) = delete;
        FreezeGuard &operator=(const FreezeGuard &other) = delete;
    private:
        Win32Console &m_console;
        bool m_previous;
    };

    Win32Console();

    HWND hwnd() { return m_hwnd; }
    std::wstring title();
    void setTitle(const std::wstring &title);
    void setFreezeUsesMark(bool useMark) { m_freezeUsesMark = useMark; }
    void setNewW10(bool isNewW10) { m_isNewW10 = isNewW10; }
    bool isNewW10() { return m_isNewW10; }
    void setFrozen(bool frozen=true);
    bool frozen() { return m_frozen; }

private:
    HWND m_hwnd = nullptr;
    bool m_frozen = false;
    bool m_freezeUsesMark = false;
    bool m_isNewW10 = false;
    std::vector<wchar_t> m_titleWorkBuf;
};

#endif // AGENT_WIN32_CONSOLE_H
