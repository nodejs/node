// Copyright (c) 2011-2012 Ryan Prichard
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

#include "DebugClient.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "winpty_snprintf.h"

const wchar_t *const kPipeName = L"\\\\.\\pipe\\DebugServer";

void *volatile g_debugConfig;

namespace {

// It would be easy to accidentally trample on the Windows LastError value
// by adding logging/debugging code.  Ensure that can't happen by saving and
// restoring the value.  This saving and restoring doesn't happen along the
// fast path.
class PreserveLastError {
public:
    PreserveLastError() : m_lastError(GetLastError()) {}
    ~PreserveLastError() { SetLastError(m_lastError); }
private:
    DWORD m_lastError;
};

} // anonymous namespace

static void sendToDebugServer(const char *message)
{
    HANDLE tracePipe = INVALID_HANDLE_VALUE;

    do {
        // The default impersonation level is SECURITY_IMPERSONATION, which allows
        // a sufficiently authorized named pipe server to impersonate the client.
        // There's no need for impersonation in this debugging system, so reduce
        // the impersonation level to SECURITY_IDENTIFICATION, which allows a
        // server to merely identify us.
        tracePipe = CreateFileW(
            kPipeName,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING,
            SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
            NULL);
    } while (tracePipe == INVALID_HANDLE_VALUE &&
             GetLastError() == ERROR_PIPE_BUSY &&
             WaitNamedPipeW(kPipeName, NMPWAIT_WAIT_FOREVER));

    if (tracePipe != INVALID_HANDLE_VALUE) {
        DWORD newMode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(tracePipe, &newMode, NULL, NULL);
        char response[16];
        DWORD actual = 0;
        TransactNamedPipe(tracePipe,
            const_cast<char*>(message), strlen(message),
            response, sizeof(response), &actual, NULL);
        CloseHandle(tracePipe);
    }
}

// Get the current UTC time as milliseconds from the epoch (ignoring leap
// seconds).  Use the Unix epoch for consistency with DebugClient.py.  There
// are 134774 days between 1601-01-01 (the Win32 epoch) and 1970-01-01 (the
// Unix epoch).
static long long unixTimeMillis()
{
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);
    long long msTime = (((long long)fileTime.dwHighDateTime << 32) +
                       fileTime.dwLowDateTime) / 10000;
    return msTime - 134774LL * 24 * 3600 * 1000;
}

static const char *getDebugConfig()
{
    if (g_debugConfig == NULL) {
        PreserveLastError preserve;
        const int bufSize = 256;
        char buf[bufSize];
        DWORD actualSize =
            GetEnvironmentVariableA("WINPTY_DEBUG", buf, bufSize);
        if (actualSize == 0 || actualSize >= static_cast<DWORD>(bufSize)) {
            buf[0] = '\0';
        }
        const size_t len = strlen(buf) + 1;
        char *newConfig = new char[len];
        std::copy(buf, buf + len, newConfig);
        void *oldValue = InterlockedCompareExchangePointer(
            &g_debugConfig, newConfig, NULL);
        if (oldValue != NULL) {
            delete [] newConfig;
        }
    }
    return static_cast<const char*>(g_debugConfig);
}

bool isTracingEnabled()
{
    static bool disabled, enabled;
    if (disabled) {
        return false;
    } else if (enabled) {
        return true;
    } else {
        // Recognize WINPTY_DEBUG=1 for backwards compatibility.
        PreserveLastError preserve;
        bool value = hasDebugFlag("trace") || hasDebugFlag("1");
        disabled = !value;
        enabled = value;
        return value;
    }
}

bool hasDebugFlag(const char *flag)
{
    if (strchr(flag, ',') != NULL) {
        trace("INTERNAL ERROR: hasDebugFlag flag has comma: '%s'", flag);
        abort();
    }
    const char *const configCStr = getDebugConfig();
    if (configCStr[0] == '\0') {
        return false;
    }
    PreserveLastError preserve;
    std::string config(configCStr);
    std::string flagStr(flag);
    config = "," + config + ",";
    flagStr = "," + flagStr + ",";
    return config.find(flagStr) != std::string::npos;
}

void trace(const char *format, ...)
{
    if (!isTracingEnabled())
        return;

    PreserveLastError preserve;
    char message[1024];

    va_list ap;
    va_start(ap, format);
    winpty_vsnprintf(message, format, ap);
    message[sizeof(message) - 1] = '\0';
    va_end(ap);

    const int currentTime = (int)(unixTimeMillis() % (100000 * 1000));

    char moduleName[1024];
    moduleName[0] = '\0';
    GetModuleFileNameA(NULL, moduleName, sizeof(moduleName));
    const char *baseName = strrchr(moduleName, '\\');
    baseName = (baseName != NULL) ? baseName + 1 : moduleName;

    char fullMessage[1024];
    winpty_snprintf(fullMessage,
             "[%05d.%03d %s,p%04d,t%04d]: %s",
             currentTime / 1000, currentTime % 1000,
             baseName, (int)GetCurrentProcessId(), (int)GetCurrentThreadId(),
             message);
    fullMessage[sizeof(fullMessage) - 1] = '\0';

    sendToDebugServer(fullMessage);
}
