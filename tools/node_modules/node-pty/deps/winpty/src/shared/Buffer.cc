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

#include "Buffer.h"

#include <stdint.h>

#include "DebugClient.h"
#include "WinptyAssert.h"

// Define the READ_BUFFER_CHECK() macro.  It *must* evaluate its condition,
// exactly once.
#define READ_BUFFER_CHECK(cond)                                 \
    do {                                                        \
        if (!(cond)) {                                          \
            trace("decode error: %s", #cond);                   \
            throw DecodeError();                                \
        }                                                       \
    } while (false)

enum class Piece : uint8_t { Int32, Int64, WString };

void WriteBuffer::putRawData(const void *data, size_t len) {
    const auto p = reinterpret_cast<const char*>(data);
    m_buf.insert(m_buf.end(), p, p + len);
}

void WriteBuffer::replaceRawData(size_t pos, const void *data, size_t len) {
    ASSERT(pos <= m_buf.size() && len <= m_buf.size() - pos);
    const auto p = reinterpret_cast<const char*>(data);
    std::copy(p, p + len, &m_buf[pos]);
}

void WriteBuffer::putInt32(int32_t i) {
    putRawValue(Piece::Int32);
    putRawValue(i);
}

void WriteBuffer::putInt64(int64_t i) {
    putRawValue(Piece::Int64);
    putRawValue(i);
}

// len is in characters, excluding NUL, i.e. the number of wchar_t elements
void WriteBuffer::putWString(const wchar_t *str, size_t len) {
    putRawValue(Piece::WString);
    putRawValue(static_cast<uint64_t>(len));
    putRawData(str, sizeof(wchar_t) * len);
}

void ReadBuffer::getRawData(void *data, size_t len) {
    ASSERT(m_off <= m_buf.size());
    READ_BUFFER_CHECK(len <= m_buf.size() - m_off);
    const char *const inp = &m_buf[m_off];
    std::copy(inp, inp + len, reinterpret_cast<char*>(data));
    m_off += len;
}

int32_t ReadBuffer::getInt32() {
    READ_BUFFER_CHECK(getRawValue<Piece>() == Piece::Int32);
    return getRawValue<int32_t>();
}

int64_t ReadBuffer::getInt64() {
    READ_BUFFER_CHECK(getRawValue<Piece>() == Piece::Int64);
    return getRawValue<int64_t>();
}

std::wstring ReadBuffer::getWString() {
    READ_BUFFER_CHECK(getRawValue<Piece>() == Piece::WString);
    const uint64_t charLen = getRawValue<uint64_t>();
    READ_BUFFER_CHECK(charLen <= SIZE_MAX / sizeof(wchar_t));
    // To be strictly conforming, we can't use the convenient wstring
    // constructor, because the string in m_buf mightn't be aligned.
    std::wstring ret;
    if (charLen > 0) {
        const size_t byteLen = charLen * sizeof(wchar_t);
        ret.resize(charLen);
        getRawData(&ret[0], byteLen);
    }
    return ret;
}

void ReadBuffer::assertEof() {
    READ_BUFFER_CHECK(m_off == m_buf.size());
}
