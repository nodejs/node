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

#ifndef WINPTY_SHARED_BUFFER_H
#define WINPTY_SHARED_BUFFER_H

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <utility>
#include <vector>
#include <string>

#include "WinptyException.h"

class WriteBuffer {
private:
    std::vector<char> m_buf;

public:
    WriteBuffer() {}

    template <typename T> void putRawValue(const T &t) {
        putRawData(&t, sizeof(t));
    }
    template <typename T> void replaceRawValue(size_t pos, const T &t) {
        replaceRawData(pos, &t, sizeof(t));
    }

    void putRawData(const void *data, size_t len);
    void replaceRawData(size_t pos, const void *data, size_t len);
    void putInt32(int32_t i);
    void putInt64(int64_t i);
    void putWString(const wchar_t *str, size_t len);
    void putWString(const wchar_t *str)         { putWString(str, wcslen(str)); }
    void putWString(const std::wstring &str)    { putWString(str.data(), str.size()); }
    std::vector<char> &buf()                    { return m_buf; }

    // MSVC 2013 does not generate these automatically, so help it out.
    WriteBuffer(WriteBuffer &&other) : m_buf(std::move(other.m_buf)) {}
    WriteBuffer &operator=(WriteBuffer &&other) {
        m_buf = std::move(other.m_buf);
        return *this;
    }
};

class ReadBuffer {
public:
    class DecodeError : public WinptyException {
        virtual const wchar_t *what() const WINPTY_NOEXCEPT override {
            return L"DecodeError: RPC message decoding error";
        }
    };

private:
    std::vector<char> m_buf;
    size_t m_off = 0;

public:
    explicit ReadBuffer(std::vector<char> &&buf) : m_buf(std::move(buf)) {}

    template <typename T> T getRawValue() {
        T ret = {};
        getRawData(&ret, sizeof(ret));
        return ret;
    }

    void getRawData(void *data, size_t len);
    int32_t getInt32();
    int64_t getInt64();
    std::wstring getWString();
    void assertEof();

    // MSVC 2013 does not generate these automatically, so help it out.
    ReadBuffer(ReadBuffer &&other) :
        m_buf(std::move(other.m_buf)), m_off(other.m_off) {}
    ReadBuffer &operator=(ReadBuffer &&other) {
        m_buf = std::move(other.m_buf);
        m_off = other.m_off;
        return *this;
    }
};

#endif // WINPTY_SHARED_BUFFER_H
