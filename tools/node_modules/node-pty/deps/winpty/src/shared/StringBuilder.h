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

// Efficient integer->string conversion and string concatenation.  The
// hexadecimal conversion may optionally have leading zeros.  Other ways to
// convert integers to strings in C++ suffer these drawbacks:
//
//  * std::stringstream: Inefficient, even more so than stdio.
//
//  * std::to_string: No hexadecimal output, tends to use heap allocation, not
//    supported on Cygwin.
//
//  * stdio routines: Requires parsing a format string (inefficient).  The
//    caller *must* know how large the content is for correctness.  The
//    string-printf functions are extremely inconsistent on Windows.  In
//    particular, 64-bit integers, wide strings, and return values are
//    problem areas.
//
// StringBuilderTest.cc is a standalone program that tests this header.

#ifndef WINPTY_STRING_BUILDER_H
#define WINPTY_STRING_BUILDER_H

#include <array>
#include <string>
#include <type_traits>

#ifdef STRING_BUILDER_TESTING
#include <assert.h>
#define STRING_BUILDER_CHECK(cond) assert(cond)
#else
#define STRING_BUILDER_CHECK(cond)
#endif // STRING_BUILDER_TESTING

#include "WinptyAssert.h"

template <typename C, size_t sz>
struct ValueString {
    std::array<C, sz> m_array;
    size_t m_offset;
    size_t m_size;

    const C *c_str() const { return m_array.data() + m_offset; }
    const C *data() const { return m_array.data() + m_offset; }
    size_t size() const { return m_size; }
    std::basic_string<C> str() const {
        return std::basic_string<C>(data(), m_size);
    }
};

#ifdef _MSC_VER
// Disable an MSVC /SDL error that forbids unsigned negation.  Signed negation
// invokes undefined behavior for INTxx_MIN, so unsigned negation is simpler to
// reason about.  (We assume twos-complement in any case.)
#define STRING_BUILDER_ALLOW_UNSIGNED_NEGATE(x) \
    (                                           \
        __pragma(warning(push))                 \
        __pragma(warning(disable:4146))         \
        (x)                                     \
        __pragma(warning(pop))                  \
    )
#else
#define STRING_BUILDER_ALLOW_UNSIGNED_NEGATE(x) (x)
#endif

// Formats an integer as decimal without leading zeros.
template <typename C, typename I>
ValueString<C, sizeof(I) * 3 + 1 + 1> gdecOfInt(const I value) {
    typedef typename std::make_unsigned<I>::type U;
    auto unsValue = static_cast<U>(value);
    const bool isNegative = (value < 0);
    if (isNegative) {
        unsValue = STRING_BUILDER_ALLOW_UNSIGNED_NEGATE(-unsValue);
    }
    decltype(gdecOfInt<C, I>(value)) out;
    auto &arr = out.m_array;
    C *const endp = arr.data() + arr.size();
    C *outp = endp;
    *(--outp) = '\0';
    STRING_BUILDER_CHECK(outp >= arr.data());
    do {
        const int digit = unsValue % 10;
        unsValue /= 10;
        *(--outp) = '0' + digit;
        STRING_BUILDER_CHECK(outp >= arr.data());
    } while (unsValue != 0);
    if (isNegative) {
        *(--outp) = '-';
        STRING_BUILDER_CHECK(outp >= arr.data());
    }
    out.m_offset = outp - arr.data();
    out.m_size = endp - outp - 1;
    return out;
}

template <typename I> decltype(gdecOfInt<char, I>(0)) decOfInt(I i) {
    return gdecOfInt<char>(i);
}

template <typename I> decltype(gdecOfInt<wchar_t, I>(0)) wdecOfInt(I i) {
    return gdecOfInt<wchar_t>(i);
}

// Formats an integer as hexadecimal, with or without leading zeros.
template <typename C, bool leadingZeros=false, typename I>
ValueString<C, sizeof(I) * 2 + 1> ghexOfInt(const I value) {
    typedef typename std::make_unsigned<I>::type U;
    const auto unsValue = static_cast<U>(value);
    static const C hex[16] = {'0','1','2','3','4','5','6','7',
                              '8','9','a','b','c','d','e','f'};
    decltype(ghexOfInt<C, leadingZeros, I>(value)) out;
    auto &arr = out.m_array;
    C *outp = arr.data();
    int inIndex = 0;
    int shift = sizeof(I) * 8 - 4;
    const int len = sizeof(I) * 2;
    if (!leadingZeros) {
        for (; inIndex < len - 1; ++inIndex, shift -= 4) {
            STRING_BUILDER_CHECK(shift >= 0 && shift < sizeof(unsValue) * 8);
            const int digit = (unsValue >> shift) & 0xF;
            if (digit != 0) {
                break;
            }
        }
    }
    for (; inIndex < len; ++inIndex, shift -= 4) {
        const int digit = (unsValue >> shift) & 0xF;
        *(outp++) = hex[digit];
        STRING_BUILDER_CHECK(outp <= arr.data() + arr.size());
    }
    *(outp++) = '\0';
    STRING_BUILDER_CHECK(outp <= arr.data() + arr.size());
    out.m_offset = 0;
    out.m_size = outp - arr.data() - 1;
    return out;
}

template <bool leadingZeros=false, typename I>
decltype(ghexOfInt<char, leadingZeros, I>(0)) hexOfInt(I i) {
    return ghexOfInt<char, leadingZeros, I>(i);
}

template <bool leadingZeros=false, typename I>
decltype(ghexOfInt<wchar_t, leadingZeros, I>(0)) whexOfInt(I i) {
    return ghexOfInt<wchar_t, leadingZeros, I>(i);
}

template <typename C>
class GStringBuilder {
public:
    typedef std::basic_string<C> StringType;

    GStringBuilder() {}
    GStringBuilder(size_t capacity) {
        m_out.reserve(capacity);
    }

    GStringBuilder &operator<<(C ch) { m_out.push_back(ch); return *this; }
    GStringBuilder &operator<<(const C *str) { m_out.append(str); return *this; }
    GStringBuilder &operator<<(const StringType &str) { m_out.append(str); return *this; }

    template <size_t sz>
    GStringBuilder &operator<<(const ValueString<C, sz> &str) {
        m_out.append(str.data(), str.size());
        return *this;
    }

private:
    // Forbid output of char/wchar_t for GStringBuilder if the type doesn't
    // exactly match the builder element type.  The code still allows
    // signed char and unsigned char, but I'm a little worried about what
    // happens if a user tries to output int8_t or uint8_t.
    template <typename P>
    typename std::enable_if<
        (std::is_same<P, char>::value || std::is_same<P, wchar_t>::value) &&
        !std::is_same<C, P>::value, GStringBuilder&>::type
    operator<<(P ch) {
        ASSERT(false && "Method was not supposed to be reachable.");
        return *this;
    }

public:
    GStringBuilder &operator<<(short i)              { return *this << gdecOfInt<C>(i); }
    GStringBuilder &operator<<(unsigned short i)     { return *this << gdecOfInt<C>(i); }
    GStringBuilder &operator<<(int i)                { return *this << gdecOfInt<C>(i); }
    GStringBuilder &operator<<(unsigned int i)       { return *this << gdecOfInt<C>(i); }
    GStringBuilder &operator<<(long i)               { return *this << gdecOfInt<C>(i); }
    GStringBuilder &operator<<(unsigned long i)      { return *this << gdecOfInt<C>(i); }
    GStringBuilder &operator<<(long long i)          { return *this << gdecOfInt<C>(i); }
    GStringBuilder &operator<<(unsigned long long i) { return *this << gdecOfInt<C>(i); }

    GStringBuilder &operator<<(const void *p) {
        m_out.push_back(static_cast<C>('0'));
        m_out.push_back(static_cast<C>('x'));
        *this << ghexOfInt<C>(reinterpret_cast<uintptr_t>(p));
        return *this;
    }

    StringType str() { return m_out; }
    StringType str_moved() { return std::move(m_out); }
    const C *c_str() const { return m_out.c_str(); }

private:
    StringType m_out;
};

typedef GStringBuilder<char> StringBuilder;
typedef GStringBuilder<wchar_t> WStringBuilder;

#endif // WINPTY_STRING_BUILDER_H
