//===-- Collection of utils for implementing wide char functions --*-C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_WCTYPE_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_WCTYPE_UTILS_H

#include "hdr/types/wchar_t.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"

#define LIBC_WCTYPE_MODE_ASCII 0
#define LIBC_WCTYPE_MODE_UTF8 1

#ifndef LIBC_CONF_WCTYPE_MODE
#define LIBC_CONF_WCTYPE_MODE LIBC_WCTYPE_MODE_ASCII
#endif

#if LIBC_CONF_WCTYPE_MODE == LIBC_WCTYPE_MODE_UTF8
#include "src/__support/wctype/wctype_classification_utils.h"
#endif

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// -----------------------------------------------------------------------------
// ******************                 WARNING                 ******************
// ****************** DO NOT TRY TO OPTIMIZE THESE FUNCTIONS! ******************
// -----------------------------------------------------------------------------
// This switch/case form is easier for the compiler to understand, and is
// optimized into a form that is almost always the same as or better than
// versions written by hand (see https://godbolt.org/z/qvrebqvvr). Also this
// form makes these functions encoding independent. If you want to rewrite these
// functions, make sure you have benchmarks to show your new solution is faster,
// as well as a way to support non-ASCII character encodings.

// Similarly, do not change these fumarks to show your new solution is faster,
// as well as a way to support non-Anctions to use case ranges. e.g.
//  bool islower(wchar_t ch) {
//    switch(ch) {
//    case L'a'...L'z':
//      return true;
//    }
//  }
// This assumes the character ranges are contiguous, which they aren't in
// EBCDIC. Technically we could use some smaller ranges, but that's even harder
// to read.
namespace ascii {
LIBC_INLINE constexpr bool islower(wchar_t wch) {
  switch (wch) {
  case L'a':
  case L'b':
  case L'c':
  case L'd':
  case L'e':
  case L'f':
  case L'g':
  case L'h':
  case L'i':
  case L'j':
  case L'k':
  case L'l':
  case L'm':
  case L'n':
  case L'o':
  case L'p':
  case L'q':
  case L'r':
  case L's':
  case L't':
  case L'u':
  case L'v':
  case L'w':
  case L'x':
  case L'y':
  case L'z':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isupper(wchar_t wch) {
  switch (wch) {
  case L'A':
  case L'B':
  case L'C':
  case L'D':
  case L'E':
  case L'F':
  case L'G':
  case L'H':
  case L'I':
  case L'J':
  case L'K':
  case L'L':
  case L'M':
  case L'N':
  case L'O':
  case L'P':
  case L'Q':
  case L'R':
  case L'S':
  case L'T':
  case L'U':
  case L'V':
  case L'W':
  case L'X':
  case L'Y':
  case L'Z':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isdigit(wchar_t wch) {
  switch (wch) {
  case L'0':
  case L'1':
  case L'2':
  case L'3':
  case L'4':
  case L'5':
  case L'6':
  case L'7':
  case L'8':
  case L'9':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isalpha(wchar_t wch) {
  switch (wch) {
  case L'a':
  case L'b':
  case L'c':
  case L'd':
  case L'e':
  case L'f':
  case L'g':
  case L'h':
  case L'i':
  case L'j':
  case L'k':
  case L'l':
  case L'm':
  case L'n':
  case L'o':
  case L'p':
  case L'q':
  case L'r':
  case L's':
  case L't':
  case L'u':
  case L'v':
  case L'w':
  case L'x':
  case L'y':
  case L'z':
  case L'A':
  case L'B':
  case L'C':
  case L'D':
  case L'E':
  case L'F':
  case L'G':
  case L'H':
  case L'I':
  case L'J':
  case L'K':
  case L'L':
  case L'M':
  case L'N':
  case L'O':
  case L'P':
  case L'Q':
  case L'R':
  case L'S':
  case L'T':
  case L'U':
  case L'V':
  case L'W':
  case L'X':
  case L'Y':
  case L'Z':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isalnum(wchar_t wch) {
  switch (wch) {
  case L'a':
  case L'b':
  case L'c':
  case L'd':
  case L'e':
  case L'f':
  case L'g':
  case L'h':
  case L'i':
  case L'j':
  case L'k':
  case L'l':
  case L'm':
  case L'n':
  case L'o':
  case L'p':
  case L'q':
  case L'r':
  case L's':
  case L't':
  case L'u':
  case L'v':
  case L'w':
  case L'x':
  case L'y':
  case L'z':
  case L'A':
  case L'B':
  case L'C':
  case L'D':
  case L'E':
  case L'F':
  case L'G':
  case L'H':
  case L'I':
  case L'J':
  case L'K':
  case L'L':
  case L'M':
  case L'N':
  case L'O':
  case L'P':
  case L'Q':
  case L'R':
  case L'S':
  case L'T':
  case L'U':
  case L'V':
  case L'W':
  case L'X':
  case L'Y':
  case L'Z':
  case L'0':
  case L'1':
  case L'2':
  case L'3':
  case L'4':
  case L'5':
  case L'6':
  case L'7':
  case L'8':
  case L'9':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isspace(wchar_t wch) {
  switch (wch) {
  case L' ':
  case L'\t':
  case L'\n':
  case L'\v':
  case L'\f':
  case L'\r':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isblank(wchar_t wch) {
  switch (wch) {
  case L' ':
  case L'\t':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isgraph(wchar_t wch) {
  return 0x20 < wch && wch < 0x7f;
}

LIBC_INLINE constexpr bool isprint(wchar_t wch) {
  return (static_cast<unsigned>(wch) - ' ') < 95;
}

LIBC_INLINE constexpr bool isxdigit(wchar_t wch) {
  switch (wch) {
  case L'a':
  case L'b':
  case L'c':
  case L'd':
  case L'e':
  case L'f':
  case L'A':
  case L'B':
  case L'C':
  case L'D':
  case L'E':
  case L'F':
  case L'0':
  case L'1':
  case L'2':
  case L'3':
  case L'4':
  case L'5':
  case L'6':
  case L'7':
  case L'8':
  case L'9':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool iscntrl(wchar_t wch) {
  return (wch < 0x20 || wch == 0x7f);
}

LIBC_INLINE constexpr bool ispunct(wchar_t wch) {
  return !isalnum(wch) && isgraph(wch);
}

LIBC_INLINE constexpr wchar_t tolower(wchar_t wch) {
  switch (wch) {
  case L'A':
    return L'a';
  case L'B':
    return L'b';
  case L'C':
    return L'c';
  case L'D':
    return L'd';
  case L'E':
    return L'e';
  case L'F':
    return L'f';
  case L'G':
    return L'g';
  case L'H':
    return L'h';
  case L'I':
    return L'i';
  case L'J':
    return L'j';
  case L'K':
    return L'k';
  case L'L':
    return L'l';
  case L'M':
    return L'm';
  case L'N':
    return L'n';
  case L'O':
    return L'o';
  case L'P':
    return L'p';
  case L'Q':
    return L'q';
  case L'R':
    return L'r';
  case L'S':
    return L's';
  case L'T':
    return L't';
  case L'U':
    return L'u';
  case L'V':
    return L'v';
  case L'W':
    return L'w';
  case L'X':
    return L'x';
  case L'Y':
    return L'y';
  case L'Z':
    return L'z';
  default:
    return wch;
  }
}

LIBC_INLINE constexpr wchar_t toupper(wchar_t wch) {
  switch (wch) {
  case L'a':
    return L'A';
  case L'b':
    return L'B';
  case L'c':
    return L'C';
  case L'd':
    return L'D';
  case L'e':
    return L'E';
  case L'f':
    return L'F';
  case L'g':
    return L'G';
  case L'h':
    return L'H';
  case L'i':
    return L'I';
  case L'j':
    return L'J';
  case L'k':
    return L'K';
  case L'l':
    return L'L';
  case L'm':
    return L'M';
  case L'n':
    return L'N';
  case L'o':
    return L'O';
  case L'p':
    return L'P';
  case L'q':
    return L'Q';
  case L'r':
    return L'R';
  case L's':
    return L'S';
  case L't':
    return L'T';
  case L'u':
    return L'U';
  case L'v':
    return L'V';
  case L'w':
    return L'W';
  case L'x':
    return L'X';
  case L'y':
    return L'Y';
  case L'z':
    return L'Z';
  default:
    return wch;
  }
}

} // namespace ascii

LIBC_INLINE constexpr bool islower(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::islower(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::islower(wch);
  }
  return lookup_properties(wch) & PropertyFlag::LOWER;
#endif
}

LIBC_INLINE constexpr bool isupper(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::isupper(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::isupper(wch);
  }
  return lookup_properties(wch) & PropertyFlag::UPPER;
#endif
}

LIBC_INLINE constexpr bool isdigit(wchar_t wch) {
  // In C.UT8, only ASCII digits are considered digits
  return ascii::isdigit(wch);
}

LIBC_INLINE constexpr bool isalpha(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::isalpha(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::isalpha(wch);
  }
  return lookup_properties(wch) & PropertyFlag::ALPHA;
#endif
}

LIBC_INLINE constexpr bool isalnum(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::isalnum(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::isalnum(wch);
  }
  // Only need to check ALPHA, digit cases are covered by ASCII path
  return lookup_properties(wch) & PropertyFlag::ALPHA;
#endif
}

LIBC_INLINE constexpr bool isspace(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::isspace(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::isspace(wch);
  }
  return lookup_properties(wch) & PropertyFlag::SPACE;
#endif
}

LIBC_INLINE constexpr bool isblank(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::isblank(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::isblank(wch);
  }
  return lookup_properties(wch) & PropertyFlag::BLANK;
#endif
}

LIBC_INLINE constexpr bool isgraph(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::isgraph(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::isgraph(wch);
  }
  // print && !space
  return (lookup_properties(wch) &
          (PropertyFlag::PRINT | PropertyFlag::SPACE)) == PropertyFlag::PRINT;
#endif
}

LIBC_INLINE constexpr bool isprint(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::isprint(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::isprint(wch);
  }
  return lookup_properties(wch) & PropertyFlag::PRINT;
#endif
}
LIBC_INLINE constexpr bool isxdigit(wchar_t wch) {
  // Hexadecimal digits are the same in C.UTF8 as in ASCII
  return ascii::isxdigit(wch);
}

LIBC_INLINE constexpr bool iscntrl(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::iscntrl(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::iscntrl(wch);
  }
  return lookup_properties(wch) & PropertyFlag::CNTRL;
#endif
}

LIBC_INLINE constexpr bool ispunct(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::ispunct(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::ispunct(wch);
  }
  return lookup_properties(wch) & PropertyFlag::PUNCT;
#endif
}

LIBC_INLINE constexpr wchar_t tolower(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::tolower(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::tolower(wch);
  }
  // TODO: Add UTF8 implementation.
  return wch;
#endif
}

LIBC_INLINE constexpr wchar_t toupper(wchar_t wch) {
#if LIBC_CONF_WCTYPE_MODE != LIBC_WCTYPE_MODE_UTF8
  return ascii::toupper(wch);
#else
  if (static_cast<uint32_t>(wch) < 128) {
    return ascii::toupper(wch);
  }
  // TODO: Add UTF8 implementation.
  return wch;
#endif
}

LIBC_INLINE constexpr int b36_char_to_int(wchar_t wch) {
  switch (wch) {
  case L'0':
    return 0;
  case L'1':
    return 1;
  case L'2':
    return 2;
  case L'3':
    return 3;
  case L'4':
    return 4;
  case L'5':
    return 5;
  case L'6':
    return 6;
  case L'7':
    return 7;
  case L'8':
    return 8;
  case L'9':
    return 9;
  case L'a':
  case L'A':
    return 10;
  case L'b':
  case L'B':
    return 11;
  case L'c':
  case L'C':
    return 12;
  case L'd':
  case L'D':
    return 13;
  case L'e':
  case L'E':
    return 14;
  case L'f':
  case L'F':
    return 15;
  case L'g':
  case L'G':
    return 16;
  case L'h':
  case L'H':
    return 17;
  case L'i':
  case L'I':
    return 18;
  case L'j':
  case L'J':
    return 19;
  case L'k':
  case L'K':
    return 20;
  case L'l':
  case L'L':
    return 21;
  case L'm':
  case L'M':
    return 22;
  case L'n':
  case L'N':
    return 23;
  case L'o':
  case L'O':
    return 24;
  case L'p':
  case L'P':
    return 25;
  case L'q':
  case L'Q':
    return 26;
  case L'r':
  case L'R':
    return 27;
  case L's':
  case L'S':
    return 28;
  case L't':
  case L'T':
    return 29;
  case L'u':
  case L'U':
    return 30;
  case L'v':
  case L'V':
    return 31;
  case L'w':
  case L'W':
    return 32;
  case L'x':
  case L'X':
    return 33;
  case L'y':
  case L'Y':
    return 34;
  case L'z':
  case L'Z':
    return 35;
  default:
    return 0;
  }
}

LIBC_INLINE static constexpr wchar_t int_to_b36_wchar(int num) {
  // Can't actually use LIBC_ASSERT here because it depends on integer_to_string
  // which depends on this.

  // LIBC_ASSERT(num < 36);
  switch (num) {
  case 0:
    return L'0';
  case 1:
    return L'1';
  case 2:
    return L'2';
  case 3:
    return L'3';
  case 4:
    return L'4';
  case 5:
    return L'5';
  case 6:
    return L'6';
  case 7:
    return L'7';
  case 8:
    return L'8';
  case 9:
    return L'9';
  case 10:
    return L'a';
  case 11:
    return L'b';
  case 12:
    return L'c';
  case 13:
    return L'd';
  case 14:
    return L'e';
  case 15:
    return L'f';
  case 16:
    return L'g';
  case 17:
    return L'h';
  case 18:
    return L'i';
  case 19:
    return L'j';
  case 20:
    return L'k';
  case 21:
    return L'l';
  case 22:
    return L'm';
  case 23:
    return L'n';
  case 24:
    return L'o';
  case 25:
    return L'p';
  case 26:
    return L'q';
  case 27:
    return L'r';
  case 28:
    return L's';
  case 29:
    return L't';
  case 30:
    return L'u';
  case 31:
    return L'v';
  case 32:
    return L'w';
  case 33:
    return L'x';
  case 34:
    return L'y';
  case 35:
    return L'z';
  default:
    return L'!';
  }
}

// An overload which provides a way to compare input with specific character
// values, when input can be of a regular or a wide character type.
LIBC_INLINE static constexpr bool
is_char_or_wchar(wchar_t ch, [[maybe_unused]] char, wchar_t wc_value) {
  return (ch == wc_value);
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_WCTYPE_UTILS_H
