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

#ifndef WINPTY_GEN_RANDOM_H
#define WINPTY_GEN_RANDOM_H

// The original MinGW requires that we include wincrypt.h.  With MinGW-w64 and
// MSVC, including windows.h is sufficient.
#include <windows.h>
#include <wincrypt.h>

#include <string>

#include "OsModule.h"

class GenRandom {
    typedef BOOLEAN WINAPI RtlGenRandom_t(PVOID, ULONG);

    OsModule m_advapi32;
    RtlGenRandom_t *m_rtlGenRandom = nullptr;
    bool m_cryptProvIsValid = false;
    HCRYPTPROV m_cryptProv = 0;

public:
    GenRandom();
    ~GenRandom();
    bool fillBuffer(void *buffer, size_t size);
    std::string randomBytes(size_t numBytes);
    std::wstring randomHexString(size_t numBytes);
    std::wstring uniqueName();

    // Return true if the crypto context was successfully initialized.
    bool valid() const {
        return m_rtlGenRandom != nullptr || m_cryptProvIsValid;
    }
};

#endif // WINPTY_GEN_RANDOM_H
