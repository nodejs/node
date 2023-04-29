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

#include "GenRandom.h"

#include <stdint.h>
#include <string.h>

#include "DebugClient.h"
#include "StringBuilder.h"

static volatile LONG g_pipeCounter;

GenRandom::GenRandom() : m_advapi32(L"advapi32.dll") {
    // First try to use the pseudo-documented RtlGenRandom function from
    // advapi32.dll.  Creating a CryptoAPI context is slow, and RtlGenRandom
    // avoids the overhead.  It's documented in this blog post[1] and on
    // MSDN[2] with a disclaimer about future breakage.  This technique is
    // apparently built-in into the MSVC CRT, though, for the rand_s function,
    // so perhaps it is stable enough.
    //
    // [1] http://blogs.msdn.com/b/michael_howard/archive/2005/01/14/353379.aspx
    // [2] https://msdn.microsoft.com/en-us/library/windows/desktop/aa387694(v=vs.85).aspx
    //
    // Both RtlGenRandom and the Crypto API functions exist in XP and up.
    m_rtlGenRandom = reinterpret_cast<RtlGenRandom_t*>(
        m_advapi32.proc("SystemFunction036"));
    // The OsModule class logs an error message if the proc is nullptr.
    if (m_rtlGenRandom != nullptr) {
        return;
    }

    // Fall back to the crypto API.
    m_cryptProvIsValid =
        CryptAcquireContext(&m_cryptProv, nullptr, nullptr,
                            PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) != 0;
    if (!m_cryptProvIsValid) {
        trace("GenRandom: CryptAcquireContext failed: %u",
            static_cast<unsigned>(GetLastError()));
    }
}

GenRandom::~GenRandom() {
    if (m_cryptProvIsValid) {
        CryptReleaseContext(m_cryptProv, 0);
    }
}

// Returns false if the context is invalid or the generation fails.
bool GenRandom::fillBuffer(void *buffer, size_t size) {
    memset(buffer, 0, size);
    bool success = false;
    if (m_rtlGenRandom != nullptr) {
        success = m_rtlGenRandom(buffer, size) != 0;
        if (!success) {
            trace("GenRandom: RtlGenRandom/SystemFunction036 failed: %u",
                static_cast<unsigned>(GetLastError()));
        }
    } else if (m_cryptProvIsValid) {
        success =
            CryptGenRandom(m_cryptProv, size,
                           reinterpret_cast<BYTE*>(buffer)) != 0;
        if (!success) {
            trace("GenRandom: CryptGenRandom failed, size=%d, lasterror=%u",
                static_cast<int>(size),
                static_cast<unsigned>(GetLastError()));
        }
    }
    return success;
}

// Returns an empty string if either of CryptAcquireContext or CryptGenRandom
// fail.
std::string GenRandom::randomBytes(size_t numBytes) {
    std::string ret(numBytes, '\0');
    if (!fillBuffer(&ret[0], numBytes)) {
        return std::string();
    }
    return ret;
}

std::wstring GenRandom::randomHexString(size_t numBytes) {
    const std::string bytes = randomBytes(numBytes);
    std::wstring ret(bytes.size() * 2, L'\0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        static const wchar_t hex[] = L"0123456789abcdef";
        ret[i * 2]     = hex[static_cast<uint8_t>(bytes[i]) >> 4];
        ret[i * 2 + 1] = hex[static_cast<uint8_t>(bytes[i]) & 0xF];
    }
    return ret;
}

// Returns a 64-bit value representing the number of 100-nanosecond intervals
// since January 1, 1601.
static uint64_t systemTimeAsUInt64() {
    FILETIME monotonicTime = {};
    GetSystemTimeAsFileTime(&monotonicTime);
    return (static_cast<uint64_t>(monotonicTime.dwHighDateTime) << 32) |
            static_cast<uint64_t>(monotonicTime.dwLowDateTime);
}

// Generates a unique and hard-to-guess case-insensitive string suitable for
// use in a pipe filename or a Windows object name.
std::wstring GenRandom::uniqueName() {
    // First include enough information to avoid collisions assuming
    // cooperative software.  This code assumes that a process won't die and
    // be replaced with a recycled PID within a single GetSystemTimeAsFileTime
    // interval.
    WStringBuilder sb(64);
    sb << GetCurrentProcessId()
       << L'-' << InterlockedIncrement(&g_pipeCounter)
       << L'-' << whexOfInt(systemTimeAsUInt64());
    // It isn't clear to me how the crypto APIs would fail.  It *probably*
    // doesn't matter that much anyway?  In principle, a predictable pipe name
    // is subject to a local denial-of-service attack.
    auto random = randomHexString(16);
    if (!random.empty()) {
        sb << L'-' << random;
    }
    return sb.str_moved();
}
