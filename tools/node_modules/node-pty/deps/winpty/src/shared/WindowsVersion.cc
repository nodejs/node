// Copyright (c) 2016 Ryan Prichard
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

#include "WindowsVersion.h"

#include <windows.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>

#include "DebugClient.h"
#include "OsModule.h"
#include "StringBuilder.h"
#include "StringUtil.h"
#include "WinptyAssert.h"
#include "WinptyException.h"

namespace {

typedef std::tuple<DWORD, DWORD> Version;

// This function can only return a version up to 6.2 unless the executable is
// manifested for a newer version of Windows.  See the MSDN documentation for
// GetVersionEx.
OSVERSIONINFOEX getWindowsVersionInfo() {
    // Allow use of deprecated functions (i.e. GetVersionEx).  We need to use
    // GetVersionEx for the old MinGW toolchain and with MSVC when it targets XP.
    // Having two code paths makes code harder to test, and it's not obvious how
    // to detect the presence of a new enough SDK.  (Including ntverp.h and
    // examining VER_PRODUCTBUILD apparently works, but even then, MinGW-w64 and
    // MSVC seem to use different version numbers.)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
    OSVERSIONINFOEX info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    const auto success = GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&info));
    ASSERT(success && "GetVersionEx failed");
    return info;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

Version getWindowsVersion() {
    const auto info = getWindowsVersionInfo();
    return Version(info.dwMajorVersion, info.dwMinorVersion);
}

struct ModuleNotFound : WinptyException {
    virtual const wchar_t *what() const WINPTY_NOEXCEPT override {
        return L"ModuleNotFound";
    }
};

// Throws WinptyException on error.
std::wstring getSystemDirectory() {
    wchar_t systemDirectory[MAX_PATH];
    const UINT size = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (size == 0) {
        throwWindowsError(L"GetSystemDirectory failed");
    } else if (size >= MAX_PATH) {
        throwWinptyException(
            L"GetSystemDirectory: path is longer than MAX_PATH");
    }
    return systemDirectory;
}

#define GET_VERSION_DLL_API(name) \
    const auto p ## name =                                  \
        reinterpret_cast<decltype(name)*>(                  \
            versionDll.proc(#name));                        \
    if (p ## name == nullptr) {                             \
        throwWinptyException(L ## #name L" is missing");    \
    }

// Throws WinptyException on error.
VS_FIXEDFILEINFO getFixedFileInfo(const std::wstring &path) {
    // version.dll is not a conventional KnownDll, so if we link to it, there's
    // a danger of accidentally loading a malicious DLL.  In a more typical
    // application, perhaps we'd guard against this security issue by
    // controlling which directories this code runs in (e.g. *not* the
    // "Downloads" directory), but that's harder for the winpty library.
    OsModule versionDll(
        (getSystemDirectory() + L"\\version.dll").c_str(),
        OsModule::LoadErrorBehavior::Throw);
    GET_VERSION_DLL_API(GetFileVersionInfoSizeW);
    GET_VERSION_DLL_API(GetFileVersionInfoW);
    GET_VERSION_DLL_API(VerQueryValueW);
    DWORD size = pGetFileVersionInfoSizeW(path.c_str(), nullptr);
    if (!size) {
        // I see ERROR_FILE_NOT_FOUND on Win7 and
        // ERROR_RESOURCE_DATA_NOT_FOUND on WinXP.
        if (GetLastError() == ERROR_FILE_NOT_FOUND ||
                GetLastError() == ERROR_RESOURCE_DATA_NOT_FOUND) {
            throw ModuleNotFound();
        } else {
            throwWindowsError(
                (L"GetFileVersionInfoSizeW failed on " + path).c_str());
        }
    }
    std::unique_ptr<char[]> versionBuffer(new char[size]);
    if (!pGetFileVersionInfoW(path.c_str(), 0, size, versionBuffer.get())) {
        throwWindowsError((L"GetFileVersionInfoW failed on " + path).c_str());
    }
    VS_FIXEDFILEINFO *versionInfo = nullptr;
    UINT versionInfoSize = 0;
    if (!pVerQueryValueW(
                versionBuffer.get(), L"\\",
                reinterpret_cast<void**>(&versionInfo), &versionInfoSize) ||
            versionInfo == nullptr ||
            versionInfoSize != sizeof(VS_FIXEDFILEINFO) ||
            versionInfo->dwSignature != 0xFEEF04BD) {
        throwWinptyException((L"VerQueryValueW failed on " + path).c_str());
    }
    return *versionInfo;
}

uint64_t productVersionFromInfo(const VS_FIXEDFILEINFO &info) {
    return (static_cast<uint64_t>(info.dwProductVersionMS) << 32) |
           (static_cast<uint64_t>(info.dwProductVersionLS));
}

uint64_t fileVersionFromInfo(const VS_FIXEDFILEINFO &info) {
    return (static_cast<uint64_t>(info.dwFileVersionMS) << 32) |
           (static_cast<uint64_t>(info.dwFileVersionLS));
}

std::string versionToString(uint64_t version) {
    StringBuilder b(32);
    b << ((uint16_t)(version >> 48));
    b << '.';
    b << ((uint16_t)(version >> 32));
    b << '.';
    b << ((uint16_t)(version >> 16));
    b << '.';
    b << ((uint16_t)(version >> 0));
    return b.str_moved();
}

} // anonymous namespace

// Returns true for Windows Vista (or Windows Server 2008) or newer.
bool isAtLeastWindowsVista() {
    return getWindowsVersion() >= Version(6, 0);
}

// Returns true for Windows 7 (or Windows Server 2008 R2) or newer.
bool isAtLeastWindows7() {
    return getWindowsVersion() >= Version(6, 1);
}

// Returns true for Windows 8 (or Windows Server 2012) or newer.
bool isAtLeastWindows8() {
    return getWindowsVersion() >= Version(6, 2);
}

#define WINPTY_IA32     1
#define WINPTY_X64      2

#if defined(_M_IX86) || defined(__i386__)
#define WINPTY_ARCH WINPTY_IA32
#elif defined(_M_X64) || defined(__x86_64__)
#define WINPTY_ARCH WINPTY_X64
#endif

typedef BOOL WINAPI IsWow64Process_t(HANDLE hProcess, PBOOL Wow64Process);

void dumpWindowsVersion() {
    if (!isTracingEnabled()) {
        return;
    }
    const auto info = getWindowsVersionInfo();
    StringBuilder b;
    b << info.dwMajorVersion << '.' << info.dwMinorVersion
      << '.' << info.dwBuildNumber << ' '
      << "SP" << info.wServicePackMajor << '.' << info.wServicePackMinor
      << ' ';
    switch (info.wProductType) {
        case VER_NT_WORKSTATION:        b << "Client"; break;
        case VER_NT_DOMAIN_CONTROLLER:  b << "DomainController"; break;
        case VER_NT_SERVER:             b << "Server"; break;
        default:
            b << "product=" << info.wProductType; break;
    }
    b << ' ';
#if WINPTY_ARCH == WINPTY_IA32
    b << "IA32";
    OsModule kernel32(L"kernel32.dll");
    IsWow64Process_t *pIsWow64Process =
        reinterpret_cast<IsWow64Process_t*>(
            kernel32.proc("IsWow64Process"));
    if (pIsWow64Process != nullptr) {
        BOOL result = false;
        const BOOL success = pIsWow64Process(GetCurrentProcess(), &result);
        if (!success) {
            b << " WOW64:error";
        } else if (success && result) {
            b << " WOW64";
        }
    } else {
        b << " WOW64:missingapi";
    }
#elif WINPTY_ARCH == WINPTY_X64
    b << "X64";
#endif
    const auto dllVersion = [](const wchar_t *dllPath) -> std::string {
        try {
            const auto info = getFixedFileInfo(dllPath);
            StringBuilder fb(64);
            fb << utf8FromWide(dllPath) << ':';
            fb << "F:" << versionToString(fileVersionFromInfo(info)) << '/'
               << "P:" << versionToString(productVersionFromInfo(info));
            return fb.str_moved();
        } catch (const ModuleNotFound&) {
            return utf8FromWide(dllPath) + ":none";
        } catch (const WinptyException &e) {
            trace("Error getting %s version: %s",
                utf8FromWide(dllPath).c_str(), utf8FromWide(e.what()).c_str());
            return utf8FromWide(dllPath) + ":error";
        }
    };
    b << ' ' << dllVersion(L"kernel32.dll");
    // ConEmu provides a DLL that hooks many Windows APIs, especially console
    // APIs.  Its existence and version number could be useful in debugging.
#if WINPTY_ARCH == WINPTY_IA32
    b << ' ' << dllVersion(L"ConEmuHk.dll");
#elif WINPTY_ARCH == WINPTY_X64
    b << ' ' << dllVersion(L"ConEmuHk64.dll");
#endif
    trace("Windows version: %s", b.c_str());
}
