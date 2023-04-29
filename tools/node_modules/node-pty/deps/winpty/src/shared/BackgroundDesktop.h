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

#ifndef WINPTY_SHARED_BACKGROUND_DESKTOP_H
#define WINPTY_SHARED_BACKGROUND_DESKTOP_H

#include <windows.h>

#include <string>

#include "WinptyException.h"

class BackgroundDesktop {
public:
    BackgroundDesktop();
    ~BackgroundDesktop() { dispose(); }
    void dispose() WINPTY_NOEXCEPT;
    const std::wstring &desktopName() const { return m_newDesktopName; }

    BackgroundDesktop(const BackgroundDesktop &other) = delete;
    BackgroundDesktop &operator=(const BackgroundDesktop &other) = delete;

    // We can't default the move constructor and assignment operator with
    // MSVC 2013.  We *could* if we required at least MSVC 2015 to build.

    BackgroundDesktop(BackgroundDesktop &&other) :
            m_originalStation(other.m_originalStation),
            m_newStation(other.m_newStation),
            m_newDesktop(other.m_newDesktop),
            m_newDesktopName(std::move(other.m_newDesktopName)) {
        other.m_originalStation = nullptr;
        other.m_newStation = nullptr;
        other.m_newDesktop = nullptr;
    }
    BackgroundDesktop &operator=(BackgroundDesktop &&other) {
        dispose();
        m_originalStation = other.m_originalStation;
        m_newStation = other.m_newStation;
        m_newDesktop = other.m_newDesktop;
        m_newDesktopName = std::move(other.m_newDesktopName);
        other.m_originalStation = nullptr;
        other.m_newStation = nullptr;
        other.m_newDesktop = nullptr;
        return *this;
    }

private:
    HWINSTA m_originalStation = nullptr;
    HWINSTA m_newStation = nullptr;
    HDESK m_newDesktop = nullptr;
    std::wstring m_newDesktopName;
};

std::wstring getCurrentDesktopName();

#endif // WINPTY_SHARED_BACKGROUND_DESKTOP_H
