// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Google Test - The Google C++ Testing and Mocking Framework
//
// This file implements a universal value printer that can print a
// value of any type T:
//
//   void ::testing::internal::UniversalPrinter<T>::Print(value, ostream_ptr);
//
// It uses the << operator when possible, and prints the bytes in the
// object otherwise.  A user can override its behavior for a class
// type Foo by defining either operator<<(::std::ostream&, const Foo&)
// or void PrintTo(const Foo&, ::std::ostream*) in the namespace that
// defines Foo.

#include "gtest/gtest-printers.h"

#include <stdio.h>

#include <cctype>
#include <cstdint>
#include <cwchar>
#include <iomanip>
#include <ios>
#include <ostream>  // NOLINT
#include <string_view>
#include <type_traits>

#include "gtest/internal/gtest-port.h"
#include "src/gtest-internal-inl.h"

namespace testing {

namespace {

using ::std::ostream;

// Prints a segment of bytes in the given object.
GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_
GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_
void PrintByteSegmentInObjectTo(const unsigned char* obj_bytes, size_t start,
                                size_t count, ostream* os) {
  char text[5] = "";
  for (size_t i = 0; i != count; i++) {
    const size_t j = start + i;
    if (i != 0) {
      // Organizes the bytes into groups of 2 for easy parsing by
      // human.
      if ((j % 2) == 0)
        *os << ' ';
      else
        *os << '-';
    }
    GTEST_SNPRINTF_(text, sizeof(text), "%02X", obj_bytes[j]);
    *os << text;
  }
}

// Prints the bytes in the given value to the given ostream.
void PrintBytesInObjectToImpl(const unsigned char* obj_bytes, size_t count,
                              ostream* os) {
  // Tells the user how big the object is.
  *os << count << "-byte object <";

  const size_t kThreshold = 132;
  const size_t kChunkSize = 64;
  // If the object size is bigger than kThreshold, we'll have to omit
  // some details by printing only the first and the last kChunkSize
  // bytes.
  if (count < kThreshold) {
    PrintByteSegmentInObjectTo(obj_bytes, 0, count, os);
  } else {
    PrintByteSegmentInObjectTo(obj_bytes, 0, kChunkSize, os);
    *os << " ... ";
    // Rounds up to 2-byte boundary.
    const size_t resume_pos = (count - kChunkSize + 1) / 2 * 2;
    PrintByteSegmentInObjectTo(obj_bytes, resume_pos, count - resume_pos, os);
  }
  *os << ">";
}

// Helpers for widening a character to char32_t. Since the standard does not
// specify if char / wchar_t is signed or unsigned, it is important to first
// convert it to the unsigned type of the same width before widening it to
// char32_t.
template <typename CharType>
char32_t ToChar32(CharType in) {
  return static_cast<char32_t>(
      static_cast<typename std::make_unsigned<CharType>::type>(in));
}

}  // namespace

namespace internal {

// Delegates to PrintBytesInObjectToImpl() to print the bytes in the
// given object.  The delegation simplifies the implementation, which
// uses the << operator and thus is easier done outside of the
// ::testing::internal namespace, which contains a << operator that
// sometimes conflicts with the one in STL.
void PrintBytesInObjectTo(const unsigned char* obj_bytes, size_t count,
                          ostream* os) {
  PrintBytesInObjectToImpl(obj_bytes, count, os);
}

// Depending on the value of a char (or wchar_t), we print it in one
// of three formats:
//   - as is if it's a printable ASCII (e.g. 'a', '2', ' '),
//   - as a hexadecimal escape sequence (e.g. '\x7F'), or
//   - as a special escape sequence (e.g. '\r', '\n').
enum CharFormat { kAsIs, kHexEscape, kSpecialEscape };

// Returns true if c is a printable ASCII character.  We test the
// value of c directly instead of calling isprint(), which is buggy on
// Windows Mobile.
inline bool IsPrintableAscii(char32_t c) { return 0x20 <= c && c <= 0x7E; }

// Prints c (of type char, char8_t, char16_t, char32_t, or wchar_t) as a
// character literal without the quotes, escaping it when necessary; returns how
// c was formatted.
template <typename Char>
static CharFormat PrintAsCharLiteralTo(Char c, ostream* os) {
  const char32_t u_c = ToChar32(c);
  switch (u_c) {
    case L'\0':
      *os << "\\0";
      break;
    case L'\'':
      *os << "\\'";
      break;
    case L'\\':
      *os << "\\\\";
      break;
    case L'\a':
      *os << "\\a";
      break;
    case L'\b':
      *os << "\\b";
      break;
    case L'\f':
      *os << "\\f";
      break;
    case L'\n':
      *os << "\\n";
      break;
    case L'\r':
      *os << "\\r";
      break;
    case L'\t':
      *os << "\\t";
      break;
    case L'\v':
      *os << "\\v";
      break;
    default:
      if (IsPrintableAscii(u_c)) {
        *os << static_cast<char>(c);
        return kAsIs;
      } else {
        ostream::fmtflags flags = os->flags();
        *os << "\\x" << std::hex << std::uppercase << static_cast<int>(u_c);
        os->flags(flags);
        return kHexEscape;
      }
  }
  return kSpecialEscape;
}

// Prints a char32_t c as if it's part of a string literal, escaping it when
// necessary; returns how c was formatted.
static CharFormat PrintAsStringLiteralTo(char32_t c, ostream* os) {
  switch (c) {
    case L'\'':
      *os << "'";
      return kAsIs;
    case L'"':
      *os << "\\\"";
      return kSpecialEscape;
    default:
      return PrintAsCharLiteralTo(c, os);
  }
}

static const char* GetCharWidthPrefix(char) { return ""; }

static const char* GetCharWidthPrefix(signed char) { return ""; }

static const char* GetCharWidthPrefix(unsigned char) { return ""; }

#ifdef __cpp_lib_char8_t
static const char* GetCharWidthPrefix(char8_t) { return "u8"; }
#endif

static const char* GetCharWidthPrefix(char16_t) { return "u"; }

static const char* GetCharWidthPrefix(char32_t) { return "U"; }

static const char* GetCharWidthPrefix(wchar_t) { return "L"; }

// Prints a char c as if it's part of a string literal, escaping it when
// necessary; returns how c was formatted.
static CharFormat PrintAsStringLiteralTo(char c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}

#ifdef __cpp_lib_char8_t
static CharFormat PrintAsStringLiteralTo(char8_t c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}
#endif

static CharFormat PrintAsStringLiteralTo(char16_t c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}

static CharFormat PrintAsStringLiteralTo(wchar_t c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}

// Prints a character c (of type char, char8_t, char16_t, char32_t, or wchar_t)
// and its code. '\0' is printed as "'\\0'", other unprintable characters are
// also properly escaped using the standard C++ escape sequence.
template <typename Char>
void PrintCharAndCodeTo(Char c, ostream* os) {
  // First, print c as a literal in the most readable form we can find.
  *os << GetCharWidthPrefix(c) << "'";
  const CharFormat format = PrintAsCharLiteralTo(c, os);
  *os << "'";

  // To aid user debugging, we also print c's code in decimal, unless
  // it's 0 (in which case c was printed as '\\0', making the code
  // obvious).
  if (c == 0) return;
  *os << " (" << static_cast<int>(c);

  // For more convenience, we print c's code again in hexadecimal,
  // unless c was already printed in the form '\x##' or the code is in
  // [1, 9].
  if (format == kHexEscape || (1 <= c && c <= 9)) {
    // Do nothing.
  } else {
    *os << ", 0x" << String::FormatHexInt(static_cast<int>(c));
  }
  *os << ")";
}

void PrintTo(unsigned char c, ::std::ostream* os) { PrintCharAndCodeTo(c, os); }
void PrintTo(signed char c, ::std::ostream* os) { PrintCharAndCodeTo(c, os); }

// Prints a wchar_t as a symbol if it is printable or as its internal
// code otherwise and also as its code.  L'\0' is printed as "L'\\0'".
void PrintTo(wchar_t wc, ostream* os) { PrintCharAndCodeTo(wc, os); }

// TODO(dcheng): Consider making this delegate to PrintCharAndCodeTo() as well.
void PrintTo(char32_t c, ::std::ostream* os) {
  *os << std::hex << "U+" << std::uppercase << std::setfill('0') << std::setw(4)
      << static_cast<uint32_t>(c);
}

// gcc/clang __{u,}int128_t
#if defined(__SIZEOF_INT128__)
void PrintTo(__uint128_t v, ::std::ostream* os) {
  if (v == 0) {
    *os << "0";
    return;
  }

  // Buffer large enough for ceil(log10(2^128))==39 and the null terminator
  char buf[40];
  char* p = buf + sizeof(buf);

  // Some configurations have a __uint128_t, but no support for built in
  // division. Do manual long division instead.

  uint64_t high = static_cast<uint64_t>(v >> 64);
  uint64_t low = static_cast<uint64_t>(v);

  *--p = 0;
  while (high != 0 || low != 0) {
    uint64_t high_mod = high % 10;
    high = high / 10;
    // This is the long division algorithm specialized for a divisor of 10 and
    // only two elements.
    // Notable values:
    //   2^64 / 10 == 1844674407370955161
    //   2^64 % 10 == 6
    const uint64_t carry = 6 * high_mod + low % 10;
    low = low / 10 + high_mod * 1844674407370955161 + carry / 10;

    char digit = static_cast<char>(carry % 10);
    *--p = static_cast<char>('0' + digit);
  }
  *os << p;
}
void PrintTo(__int128_t v, ::std::ostream* os) {
  __uint128_t uv = static_cast<__uint128_t>(v);
  if (v < 0) {
    *os << "-";
    uv = -uv;
  }
  PrintTo(uv, os);
}
#endif  // __SIZEOF_INT128__

// Prints the given array of characters to the ostream.  CharType must be either
// char, char8_t, char16_t, char32_t, or wchar_t.
// The array starts at begin (which may be nullptr) and contains len characters.
// The array may include '\0' characters and may not be NUL-terminated.
template <typename CharType>
GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_ GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
    GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
        GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_ static CharFormat
        PrintCharsAsStringTo(const CharType* begin, size_t len, ostream* os) {
  const char* const quote_prefix = GetCharWidthPrefix(CharType());
  *os << quote_prefix << "\"";
  bool is_previous_hex = false;
  CharFormat print_format = kAsIs;
  for (size_t index = 0; index < len; ++index) {
    const CharType cur = begin[index];
    if (is_previous_hex && IsXDigit(cur)) {
      // Previous character is of '\x..' form and this character can be
      // interpreted as another hexadecimal digit in its number. Break string to
      // disambiguate.
      *os << "\" " << quote_prefix << "\"";
    }
    is_previous_hex = PrintAsStringLiteralTo(cur, os) == kHexEscape;
    // Remember if any characters required hex escaping.
    if (is_previous_hex) {
      print_format = kHexEscape;
    }
  }
  *os << "\"";
  return print_format;
}

// Prints a (const) char/wchar_t array of 'len' elements, starting at address
// 'begin'.  CharType must be either char or wchar_t.
template <typename CharType>
GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_ GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
    GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
        GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_ static void
        UniversalPrintCharArray(const CharType* begin, size_t len,
                                ostream* os) {
  // The code
  //   const char kFoo[] = "foo";
  // generates an array of 4, not 3, elements, with the last one being '\0'.
  //
  // Therefore when printing a char array, we don't print the last element if
  // it's '\0', such that the output matches the string literal as it's
  // written in the source code.
  if (len > 0 && begin[len - 1] == '\0') {
    PrintCharsAsStringTo(begin, len - 1, os);
    return;
  }

  // If, however, the last element in the array is not '\0', e.g.
  //    const char kFoo[] = { 'f', 'o', 'o' };
  // we must print the entire array.  We also print a message to indicate
  // that the array is not NUL-terminated.
  PrintCharsAsStringTo(begin, len, os);
  *os << " (no terminating NUL)";
}

// Prints a (const) char array of 'len' elements, starting at address 'begin'.
void UniversalPrintArray(const char* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

#ifdef __cpp_lib_char8_t
// Prints a (const) char8_t array of 'len' elements, starting at address
// 'begin'.
void UniversalPrintArray(const char8_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}
#endif

// Prints a (const) char16_t array of 'len' elements, starting at address
// 'begin'.
void UniversalPrintArray(const char16_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

// Prints a (const) char32_t array of 'len' elements, starting at address
// 'begin'.
void UniversalPrintArray(const char32_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

// Prints a (const) wchar_t array of 'len' elements, starting at address
// 'begin'.
void UniversalPrintArray(const wchar_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

namespace {

// Prints a null-terminated C-style string to the ostream.
template <typename Char>
void PrintCStringTo(const Char* s, ostream* os) {
  if (s == nullptr) {
    *os << "NULL";
  } else {
    *os << ImplicitCast_<const void*>(s) << " pointing to ";
    PrintCharsAsStringTo(s, std::char_traits<Char>::length(s), os);
  }
}

}  // anonymous namespace

void PrintTo(const char* s, ostream* os) { PrintCStringTo(s, os); }

#ifdef __cpp_lib_char8_t
void PrintTo(const char8_t* s, ostream* os) { PrintCStringTo(s, os); }
#endif

void PrintTo(const char16_t* s, ostream* os) { PrintCStringTo(s, os); }

void PrintTo(const char32_t* s, ostream* os) { PrintCStringTo(s, os); }

// MSVC compiler can be configured to define whar_t as a typedef
// of unsigned short. Defining an overload for const wchar_t* in that case
// would cause pointers to unsigned shorts be printed as wide strings,
// possibly accessing more memory than intended and causing invalid
// memory accesses. MSVC defines _NATIVE_WCHAR_T_DEFINED symbol when
// wchar_t is implemented as a native type.
#if !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED)
// Prints the given wide C string to the ostream.
void PrintTo(const wchar_t* s, ostream* os) { PrintCStringTo(s, os); }
#endif  // wchar_t is native

namespace {

bool ContainsUnprintableControlCodes(const char* str, size_t length) {
  const unsigned char* s = reinterpret_cast<const unsigned char*>(str);

  for (size_t i = 0; i < length; i++) {
    unsigned char ch = *s++;
    if (std::iscntrl(ch)) {
      switch (ch) {
        case '\t':
        case '\n':
        case '\r':
          break;
        default:
          return true;
      }
    }
  }
  return false;
}

bool IsUTF8TrailByte(unsigned char t) { return 0x80 <= t && t <= 0xbf; }

bool IsValidUTF8(const char* str, size_t length) {
  const unsigned char* s = reinterpret_cast<const unsigned char*>(str);

  for (size_t i = 0; i < length;) {
    unsigned char lead = s[i++];

    if (lead <= 0x7f) {
      continue;  // single-byte character (ASCII) 0..7F
    }
    if (lead < 0xc2) {
      return false;  // trail byte or non-shortest form
    } else if (lead <= 0xdf && (i + 1) <= length && IsUTF8TrailByte(s[i])) {
      ++i;  // 2-byte character
    } else if (0xe0 <= lead && lead <= 0xef && (i + 2) <= length &&
               IsUTF8TrailByte(s[i]) && IsUTF8TrailByte(s[i + 1]) &&
               // check for non-shortest form and surrogate
               (lead != 0xe0 || s[i] >= 0xa0) &&
               (lead != 0xed || s[i] < 0xa0)) {
      i += 2;  // 3-byte character
    } else if (0xf0 <= lead && lead <= 0xf4 && (i + 3) <= length &&
               IsUTF8TrailByte(s[i]) && IsUTF8TrailByte(s[i + 1]) &&
               IsUTF8TrailByte(s[i + 2]) &&
               // check for non-shortest form
               (lead != 0xf0 || s[i] >= 0x90) &&
               (lead != 0xf4 || s[i] < 0x90)) {
      i += 3;  // 4-byte character
    } else {
      return false;
    }
  }
  return true;
}

void ConditionalPrintAsText(const char* str, size_t length, ostream* os) {
  if (!ContainsUnprintableControlCodes(str, length) &&
      IsValidUTF8(str, length)) {
    *os << "\n    As Text: \"" << ::std::string_view(str, length) << "\"";
  }
}

}  // anonymous namespace

void PrintStringTo(::std::string_view s, ostream* os) {
  if (PrintCharsAsStringTo(s.data(), s.size(), os) == kHexEscape) {
    if (GTEST_FLAG_GET(print_utf8)) {
      ConditionalPrintAsText(s.data(), s.size(), os);
    }
  }
}

#ifdef __cpp_lib_char8_t
void PrintU8StringTo(::std::u8string_view s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}
#endif

void PrintU16StringTo(::std::u16string_view s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}

void PrintU32StringTo(::std::u32string_view s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}

#if GTEST_HAS_STD_WSTRING
void PrintWideStringTo(::std::wstring_view s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}
#endif  // GTEST_HAS_STD_WSTRING

}  // namespace internal

}  // namespace testing
