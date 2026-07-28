//===-- Collection of utils for implementing ctype functions-------*-C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CTYPE_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_CTYPE_UTILS_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

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

// Similarly, do not change these functions to use case ranges. e.g.
//  bool islower(char ch) {
//    switch(ch) {
//    case 'a'...'z':
//      return true;
//    }
//  }
// This assumes the character ranges are contiguous, which they aren't in
// EBCDIC. Technically we could use some smaller ranges, but that's even harder
// to read.

LIBC_INLINE constexpr bool islower(char ch) {
  switch (ch) {
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
  case 'g':
  case 'h':
  case 'i':
  case 'j':
  case 'k':
  case 'l':
  case 'm':
  case 'n':
  case 'o':
  case 'p':
  case 'q':
  case 'r':
  case 's':
  case 't':
  case 'u':
  case 'v':
  case 'w':
  case 'x':
  case 'y':
  case 'z':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isupper(char ch) {
  switch (ch) {
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
  case 'G':
  case 'H':
  case 'I':
  case 'J':
  case 'K':
  case 'L':
  case 'M':
  case 'N':
  case 'O':
  case 'P':
  case 'Q':
  case 'R':
  case 'S':
  case 'T':
  case 'U':
  case 'V':
  case 'W':
  case 'X':
  case 'Y':
  case 'Z':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isdigit(char ch) {
  switch (ch) {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr char tolower(char ch) {
  switch (ch) {
  case 'A':
    return 'a';
  case 'B':
    return 'b';
  case 'C':
    return 'c';
  case 'D':
    return 'd';
  case 'E':
    return 'e';
  case 'F':
    return 'f';
  case 'G':
    return 'g';
  case 'H':
    return 'h';
  case 'I':
    return 'i';
  case 'J':
    return 'j';
  case 'K':
    return 'k';
  case 'L':
    return 'l';
  case 'M':
    return 'm';
  case 'N':
    return 'n';
  case 'O':
    return 'o';
  case 'P':
    return 'p';
  case 'Q':
    return 'q';
  case 'R':
    return 'r';
  case 'S':
    return 's';
  case 'T':
    return 't';
  case 'U':
    return 'u';
  case 'V':
    return 'v';
  case 'W':
    return 'w';
  case 'X':
    return 'x';
  case 'Y':
    return 'y';
  case 'Z':
    return 'z';
  default:
    return ch;
  }
}

LIBC_INLINE constexpr char toupper(char ch) {
  switch (ch) {
  case 'a':
    return 'A';
  case 'b':
    return 'B';
  case 'c':
    return 'C';
  case 'd':
    return 'D';
  case 'e':
    return 'E';
  case 'f':
    return 'F';
  case 'g':
    return 'G';
  case 'h':
    return 'H';
  case 'i':
    return 'I';
  case 'j':
    return 'J';
  case 'k':
    return 'K';
  case 'l':
    return 'L';
  case 'm':
    return 'M';
  case 'n':
    return 'N';
  case 'o':
    return 'O';
  case 'p':
    return 'P';
  case 'q':
    return 'Q';
  case 'r':
    return 'R';
  case 's':
    return 'S';
  case 't':
    return 'T';
  case 'u':
    return 'U';
  case 'v':
    return 'V';
  case 'w':
    return 'W';
  case 'x':
    return 'X';
  case 'y':
    return 'Y';
  case 'z':
    return 'Z';
  default:
    return ch;
  }
}

LIBC_INLINE constexpr bool isalpha(char ch) {
  switch (ch) {
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
  case 'g':
  case 'h':
  case 'i':
  case 'j':
  case 'k':
  case 'l':
  case 'm':
  case 'n':
  case 'o':
  case 'p':
  case 'q':
  case 'r':
  case 's':
  case 't':
  case 'u':
  case 'v':
  case 'w':
  case 'x':
  case 'y':
  case 'z':
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
  case 'G':
  case 'H':
  case 'I':
  case 'J':
  case 'K':
  case 'L':
  case 'M':
  case 'N':
  case 'O':
  case 'P':
  case 'Q':
  case 'R':
  case 'S':
  case 'T':
  case 'U':
  case 'V':
  case 'W':
  case 'X':
  case 'Y':
  case 'Z':
    return true;
  default:
    return false;
  }
}

LIBC_INLINE constexpr bool isalnum(char ch) {
  switch (ch) {
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
  case 'g':
  case 'h':
  case 'i':
  case 'j':
  case 'k':
  case 'l':
  case 'm':
  case 'n':
  case 'o':
  case 'p':
  case 'q':
  case 'r':
  case 's':
  case 't':
  case 'u':
  case 'v':
  case 'w':
  case 'x':
  case 'y':
  case 'z':
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
  case 'G':
  case 'H':
  case 'I':
  case 'J':
  case 'K':
  case 'L':
  case 'M':
  case 'N':
  case 'O':
  case 'P':
  case 'Q':
  case 'R':
  case 'S':
  case 'T':
  case 'U':
  case 'V':
  case 'W':
  case 'X':
  case 'Y':
  case 'Z':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return true;
  default:
    return false;
  }
}

#ifndef LIBC_COPT_CTYPE_SMALLER_ASCII
LIBC_INLINE constexpr int b36_char_to_int(char ch) {
  switch (ch) {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '4':
    return 4;
  case '5':
    return 5;
  case '6':
    return 6;
  case '7':
    return 7;
  case '8':
    return 8;
  case '9':
    return 9;
  case 'a':
  case 'A':
    return 10;
  case 'b':
  case 'B':
    return 11;
  case 'c':
  case 'C':
    return 12;
  case 'd':
  case 'D':
    return 13;
  case 'e':
  case 'E':
    return 14;
  case 'f':
  case 'F':
    return 15;
  case 'g':
  case 'G':
    return 16;
  case 'h':
  case 'H':
    return 17;
  case 'i':
  case 'I':
    return 18;
  case 'j':
  case 'J':
    return 19;
  case 'k':
  case 'K':
    return 20;
  case 'l':
  case 'L':
    return 21;
  case 'm':
  case 'M':
    return 22;
  case 'n':
  case 'N':
    return 23;
  case 'o':
  case 'O':
    return 24;
  case 'p':
  case 'P':
    return 25;
  case 'q':
  case 'Q':
    return 26;
  case 'r':
  case 'R':
    return 27;
  case 's':
  case 'S':
    return 28;
  case 't':
  case 'T':
    return 29;
  case 'u':
  case 'U':
    return 30;
  case 'v':
  case 'V':
    return 31;
  case 'w':
  case 'W':
    return 32;
  case 'x':
  case 'X':
    return 33;
  case 'y':
  case 'Y':
    return 34;
  case 'z':
  case 'Z':
    return 35;
  default:
    return 0;
  }
}
#else  // LIBC_COPT_SMALL_ASCII_CTYPE
// This version assumes ASCII for the tolower, but generates smaller code since
// the switch version of this function ends up with a table. This should only be
// used when the target is known to be ASCII.
LIBC_INLINE constexpr int b36_char_to_int(char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  char ch_unsafe_lower = ch | 32;
  if (ch_unsafe_lower >= 'a' && ch_unsafe_lower <= 'z')
    return ch_unsafe_lower - 'a' + 10;
  return 0;
}
#endif // LIBC_COPT_SMALL_ASCII_CTYPE

LIBC_INLINE constexpr char int_to_b36_char(int num) {
  // Can't actually use LIBC_ASSERT here because it depends on integer_to_string
  // which depends on this.

  // LIBC_ASSERT(num < 36);
  switch (num) {
  case 0:
    return '0';
  case 1:
    return '1';
  case 2:
    return '2';
  case 3:
    return '3';
  case 4:
    return '4';
  case 5:
    return '5';
  case 6:
    return '6';
  case 7:
    return '7';
  case 8:
    return '8';
  case 9:
    return '9';
  case 10:
    return 'a';
  case 11:
    return 'b';
  case 12:
    return 'c';
  case 13:
    return 'd';
  case 14:
    return 'e';
  case 15:
    return 'f';
  case 16:
    return 'g';
  case 17:
    return 'h';
  case 18:
    return 'i';
  case 19:
    return 'j';
  case 20:
    return 'k';
  case 21:
    return 'l';
  case 22:
    return 'm';
  case 23:
    return 'n';
  case 24:
    return 'o';
  case 25:
    return 'p';
  case 26:
    return 'q';
  case 27:
    return 'r';
  case 28:
    return 's';
  case 29:
    return 't';
  case 30:
    return 'u';
  case 31:
    return 'v';
  case 32:
    return 'w';
  case 33:
    return 'x';
  case 34:
    return 'y';
  case 35:
    return 'z';
  default:
    return '!';
  }
}

LIBC_INLINE constexpr bool isspace(char ch) {
  switch (ch) {
  case ' ':
  case '\t':
  case '\n':
  case '\v':
  case '\f':
  case '\r':
    return true;
  default:
    return false;
  }
}

// not yet encoding independent.
LIBC_INLINE constexpr bool isgraph(char ch) { return 0x20 < ch && ch < 0x7f; }

// An overload which provides a way to compare input with specific character
// values, when input can be of a regular or a wide character type.
LIBC_INLINE constexpr bool is_char_or_wchar(char ch, char c_value,
                                            [[maybe_unused]] wchar_t) {
  return (ch == c_value);
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif //  LLVM_LIBC_SRC___SUPPORT_CTYPE_UTILS_H
