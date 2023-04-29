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

#ifndef WINPTY_SHARED_OWNED_HANDLE_H
#define WINPTY_SHARED_OWNED_HANDLE_H

#include <windows.h>

class OwnedHandle {
    HANDLE m_h;
public:
    OwnedHandle() : m_h(nullptr) {}
    explicit OwnedHandle(HANDLE h) : m_h(h) {}
    ~OwnedHandle() { dispose(true); }
    void dispose(bool nothrow=false);
    HANDLE get() const { return m_h; }
    HANDLE release() { HANDLE ret = m_h; m_h = nullptr; return ret; }
    OwnedHandle(const OwnedHandle &other) = delete;
    OwnedHandle(OwnedHandle &&other) : m_h(other.release()) {}
    OwnedHandle &operator=(const OwnedHandle &other) = delete;
    OwnedHandle &operator=(OwnedHandle &&other) {
        dispose();
        m_h = other.release();
        return *this;
    }
};

#endif // WINPTY_SHARED_OWNED_HANDLE_H
