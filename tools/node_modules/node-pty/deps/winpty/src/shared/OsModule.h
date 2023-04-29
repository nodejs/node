// Copyright (c) 2015 Ryan Prichard
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

#ifndef WINPTY_SHARED_OS_MODULE_H
#define WINPTY_SHARED_OS_MODULE_H

#include <windows.h>

#include <string>

#include "DebugClient.h"
#include "WinptyAssert.h"
#include "WinptyException.h"

class OsModule {
    HMODULE m_module;
public:
    enum class LoadErrorBehavior { Abort, Throw };
    OsModule(const wchar_t *fileName,
            LoadErrorBehavior behavior=LoadErrorBehavior::Abort) {
        m_module = LoadLibraryW(fileName);
        if (behavior == LoadErrorBehavior::Abort) {
            ASSERT(m_module != NULL);
        } else {
            if (m_module == nullptr) {
                const auto err = GetLastError();
                throwWindowsError(
                    (L"LoadLibraryW error: " + std::wstring(fileName)).c_str(),
                    err);
            }
        }
    }
    ~OsModule() {
        FreeLibrary(m_module);
    }
    HMODULE handle() const { return m_module; }
    FARPROC proc(const char *funcName) {
        FARPROC ret = GetProcAddress(m_module, funcName);
        if (ret == NULL) {
            trace("GetProcAddress: %s is missing", funcName);
        }
        return ret;
    }
};

#endif // WINPTY_SHARED_OS_MODULE_H
