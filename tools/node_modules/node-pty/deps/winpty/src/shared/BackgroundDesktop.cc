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

#include "BackgroundDesktop.h"

#include <memory>

#include "DebugClient.h"
#include "StringUtil.h"
#include "WinptyException.h"

namespace {

static std::wstring getObjectName(HANDLE object) {
    BOOL success;
    DWORD lengthNeeded = 0;
    GetUserObjectInformationW(object, UOI_NAME,
                              nullptr, 0,
                              &lengthNeeded);
    ASSERT(lengthNeeded % sizeof(wchar_t) == 0);
    std::unique_ptr<wchar_t[]> tmp(
        new wchar_t[lengthNeeded / sizeof(wchar_t)]);
    success = GetUserObjectInformationW(object, UOI_NAME,
                                        tmp.get(), lengthNeeded,
                                        nullptr);
    if (!success) {
        throwWindowsError(L"GetUserObjectInformationW failed");
    }
    return std::wstring(tmp.get());
}

static std::wstring getDesktopName(HWINSTA winsta, HDESK desk) {
    return getObjectName(winsta) + L"\\" + getObjectName(desk);
}

} // anonymous namespace

// Get a non-interactive window station for the agent.
// TODO: review security w.r.t. windowstation and desktop.
BackgroundDesktop::BackgroundDesktop() {
    try {
        m_originalStation = GetProcessWindowStation();
        if (m_originalStation == nullptr) {
            throwWindowsError(
                L"BackgroundDesktop ctor: "
                L"GetProcessWindowStation returned NULL");
        }
        m_newStation =
            CreateWindowStationW(nullptr, 0, WINSTA_ALL_ACCESS, nullptr);
        if (m_newStation == nullptr) {
            throwWindowsError(
                L"BackgroundDesktop ctor: CreateWindowStationW returned NULL");
        }
        if (!SetProcessWindowStation(m_newStation)) {
            throwWindowsError(
                L"BackgroundDesktop ctor: SetProcessWindowStation failed");
        }
        m_newDesktop = CreateDesktopW(
            L"Default", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
        if (m_newDesktop == nullptr) {
            throwWindowsError(
                L"BackgroundDesktop ctor: CreateDesktopW failed");
        }
        m_newDesktopName = getDesktopName(m_newStation, m_newDesktop);
        TRACE("Created background desktop: %s",
            utf8FromWide(m_newDesktopName).c_str());
    } catch (...) {
        dispose();
        throw;
    }
}

void BackgroundDesktop::dispose() WINPTY_NOEXCEPT {
    if (m_originalStation != nullptr) {
        SetProcessWindowStation(m_originalStation);
        m_originalStation = nullptr;
    }
    if (m_newDesktop != nullptr) {
        CloseDesktop(m_newDesktop);
        m_newDesktop = nullptr;
    }
    if (m_newStation != nullptr) {
        CloseWindowStation(m_newStation);
        m_newStation = nullptr;
    }
}

std::wstring getCurrentDesktopName() {
    // MSDN says that the handles returned by GetProcessWindowStation and
    // GetThreadDesktop do not need to be passed to CloseWindowStation and
    // CloseDesktop, respectively.
    const HWINSTA winsta = GetProcessWindowStation();
    if (winsta == nullptr) {
        throwWindowsError(
            L"getCurrentDesktopName: "
            L"GetProcessWindowStation returned NULL");
    }
    const HDESK desk = GetThreadDesktop(GetCurrentThreadId());
    if (desk == nullptr) {
        throwWindowsError(
            L"getCurrentDesktopName: "
            L"GetThreadDesktop returned NULL");
    }
    return getDesktopName(winsta, desk);
}
