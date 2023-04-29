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

#ifndef LIB_WINPTY_EXCEPTION_H
#define LIB_WINPTY_EXCEPTION_H

#include "../include/winpty.h"

#include "../shared/WinptyException.h"

#include <memory>
#include <string>

class LibWinptyException : public WinptyException {
public:
    LibWinptyException(winpty_result_t code, const wchar_t *what) :
        m_code(code), m_what(std::make_shared<std::wstring>(what)) {}

    winpty_result_t code() const WINPTY_NOEXCEPT {
        return m_code;
    }

    const wchar_t *what() const WINPTY_NOEXCEPT override {
        return m_what->c_str();
    }

    std::shared_ptr<std::wstring> whatSharedStr() const WINPTY_NOEXCEPT {
        return m_what;
    }

private:
    winpty_result_t m_code;
    // Using a shared_ptr ensures that copying the object raises no exception.
    std::shared_ptr<std::wstring> m_what;
};

#endif // LIB_WINPTY_EXCEPTION_H
