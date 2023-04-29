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

#define STRING_BUILDER_TESTING

#include "StringBuilder.h"

#include <stdio.h>
#include <string.h>

#include <iomanip>
#include <sstream>

void display(const std::string &str) { fprintf(stderr, "%s", str.c_str()); }
void display(const std::wstring &str) { fprintf(stderr, "%ls", str.c_str()); }

#define CHECK_EQ(x, y) \
    do {                                                    \
        const auto xval = (x);                              \
        const auto yval = (y);                              \
        if (xval != yval) {                                 \
            fprintf(stderr, "error: %s:%d: %s != %s: ",     \
                __FILE__, __LINE__, #x, #y);                \
            display(xval);                                  \
            fprintf(stderr, " != ");                        \
            display(yval);                                  \
            fprintf(stderr, "\n");                          \
        }                                                   \
    } while(0)

template <typename C, typename I>
std::basic_string<C> decOfIntSS(const I value) {
    // std::to_string and std::to_wstring are missing in Cygwin as of this
    // writing (early 2016).
    std::basic_stringstream<C> ss;
    ss << +value; // We must promote char to print it as an integer.
    return ss.str();
}


template <typename C, bool leadingZeros=false, typename I>
std::basic_string<C> hexOfIntSS(const I value) {
    typedef typename std::make_unsigned<I>::type U;
    const unsigned long long u64Value = value & static_cast<U>(~0);
    std::basic_stringstream<C> ss;
    if (leadingZeros) {
        ss << std::setfill(static_cast<C>('0')) << std::setw(sizeof(I) * 2);
    }
    ss << std::hex << u64Value;
    return ss.str();
}

template <typename I>
void testValue(I value) {
    CHECK_EQ(decOfInt(value).str(), (decOfIntSS<char>(value)));
    CHECK_EQ(wdecOfInt(value).str(), (decOfIntSS<wchar_t>(value)));
    CHECK_EQ((hexOfInt<false>(value).str()), (hexOfIntSS<char, false>(value)));
    CHECK_EQ((hexOfInt<true>(value).str()), (hexOfIntSS<char, true>(value)));
    CHECK_EQ((whexOfInt<false>(value).str()), (hexOfIntSS<wchar_t, false>(value)));
    CHECK_EQ((whexOfInt<true>(value).str()), (hexOfIntSS<wchar_t, true>(value)));
}

template <typename I>
void testType() {
    typedef typename std::make_unsigned<I>::type U;
    const U quarter = static_cast<U>(1) << (sizeof(U) * 8 - 2);
    for (unsigned quarterIndex = 0; quarterIndex < 4; ++quarterIndex) {
        for (int offset = -18; offset <= 18; ++offset) {
            const I value = quarter * quarterIndex + static_cast<U>(offset);
            testValue(value);
        }
    }
    testValue(static_cast<I>(42));
    testValue(static_cast<I>(123456));
    testValue(static_cast<I>(0xdeadfacecafebeefull));
}

int main() {
    testType<char>();

    testType<signed char>();
    testType<signed short>();
    testType<signed int>();
    testType<signed long>();
    testType<signed long long>();

    testType<unsigned char>();
    testType<unsigned short>();
    testType<unsigned int>();
    testType<unsigned long>();
    testType<unsigned long long>();

    StringBuilder() << static_cast<const void*>("TEST");
    WStringBuilder() << static_cast<const void*>("TEST");

    fprintf(stderr, "All tests completed!\n");
}
