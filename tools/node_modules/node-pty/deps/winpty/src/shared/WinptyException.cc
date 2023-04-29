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

#include "WinptyException.h"

#include <memory>
#include <string>

#include "StringBuilder.h"

namespace {

class ExceptionImpl : public WinptyException {
public:
    ExceptionImpl(const wchar_t *what) :
        m_what(std::make_shared<std::wstring>(what)) {}
    virtual const wchar_t *what() const WINPTY_NOEXCEPT override {
        return m_what->c_str();
    }
private:
    // Using a shared_ptr ensures that copying the object raises no exception.
    std::shared_ptr<std::wstring> m_what;
};

} // anonymous namespace

void throwWinptyException(const wchar_t *what) {
    throw ExceptionImpl(what);
}

void throwWindowsError(const wchar_t *prefix, DWORD errorCode) {
    WStringBuilder sb(64);
    if (prefix != nullptr) {
        sb << prefix << L": ";
    }
    // It might make sense to use FormatMessage here, but IIRC, its API is hard
    // to figure out.
    sb << L"Windows error " << errorCode;
    throwWinptyException(sb.c_str());
}
