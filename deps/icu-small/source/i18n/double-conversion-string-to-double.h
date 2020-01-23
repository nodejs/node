// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
// From the double-conversion library. Original license:
//
// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

// ICU PATCH: ifdef around UCONFIG_NO_FORMATTING
#include "unicode/utypes.h"
#if !UCONFIG_NO_FORMATTING

#ifndef DOUBLE_CONVERSION_STRING_TO_DOUBLE_H_
#define DOUBLE_CONVERSION_STRING_TO_DOUBLE_H_

// ICU PATCH: Customize header file paths for ICU.

#include "double-conversion-utils.h"

// ICU PATCH: Wrap in ICU namespace
U_NAMESPACE_BEGIN

namespace double_conversion {

class StringToDoubleConverter {
 public:
  // Enumeration for allowing octals and ignoring junk when converting
  // strings to numbers.
  enum Flags {
    NO_FLAGS = 0,
    ALLOW_HEX = 1,
    ALLOW_OCTALS = 2,
    ALLOW_TRAILING_JUNK = 4,
    ALLOW_LEADING_SPACES = 8,
    ALLOW_TRAILING_SPACES = 16,
    ALLOW_SPACES_AFTER_SIGN = 32,
    ALLOW_CASE_INSENSITIVITY = 64,
    ALLOW_CASE_INSENSIBILITY = 64,  // Deprecated
    ALLOW_HEX_FLOATS = 128,
  };

  static const uc16 kNoSeparator = '\0';

  // Flags should be a bit-or combination of the possible Flags-enum.
  //  - NO_FLAGS: no special flags.
  //  - ALLOW_HEX: recognizes the prefix "0x". Hex numbers may only be integers.
  //      Ex: StringToDouble("0x1234") -> 4660.0
  //          In StringToDouble("0x1234.56") the characters ".56" are trailing
  //          junk. The result of the call is hence dependent on
  //          the ALLOW_TRAILING_JUNK flag and/or the junk value.
  //      With this flag "0x" is a junk-string. Even with ALLOW_TRAILING_JUNK,
  //      the string will not be parsed as "0" followed by junk.
  //
  //  - ALLOW_OCTALS: recognizes the prefix "0" for octals:
  //      If a sequence of octal digits starts with '0', then the number is
  //      read as octal integer. Octal numbers may only be integers.
  //      Ex: StringToDouble("01234") -> 668.0
  //          StringToDouble("012349") -> 12349.0  // Not a sequence of octal
  //                                               // digits.
  //          In StringToDouble("01234.56") the characters ".56" are trailing
  //          junk. The result of the call is hence dependent on
  //          the ALLOW_TRAILING_JUNK flag and/or the junk value.
  //          In StringToDouble("01234e56") the characters "e56" are trailing
  //          junk, too.
  //  - ALLOW_TRAILING_JUNK: ignore trailing characters that are not part of
  //      a double literal.
  //  - ALLOW_LEADING_SPACES: skip over leading whitespace, including spaces,
  //                          new-lines, and tabs.
  //  - ALLOW_TRAILING_SPACES: ignore trailing whitespace.
  //  - ALLOW_SPACES_AFTER_SIGN: ignore whitespace after the sign.
  //       Ex: StringToDouble("-   123.2") -> -123.2.
  //           StringToDouble("+   123.2") -> 123.2
  //  - ALLOW_CASE_INSENSITIVITY: ignore case of characters for special values:
  //      infinity and nan.
  //  - ALLOW_HEX_FLOATS: allows hexadecimal float literals.
  //      This *must* start with "0x" and separate the exponent with "p".
  //      Examples: 0x1.2p3 == 9.0
  //                0x10.1p0 == 16.0625
  //      ALLOW_HEX and ALLOW_HEX_FLOATS are indendent.
  //
  // empty_string_value is returned when an empty string is given as input.
  // If ALLOW_LEADING_SPACES or ALLOW_TRAILING_SPACES are set, then a string
  // containing only spaces is converted to the 'empty_string_value', too.
  //
  // junk_string_value is returned when
  //  a) ALLOW_TRAILING_JUNK is not set, and a junk character (a character not
  //     part of a double-literal) is found.
  //  b) ALLOW_TRAILING_JUNK is set, but the string does not start with a
  //     double literal.
  //
  // infinity_symbol and nan_symbol are strings that are used to detect
  // inputs that represent infinity and NaN. They can be null, in which case
  // they are ignored.
  // The conversion routine first reads any possible signs. Then it compares the
  // following character of the input-string with the first character of
  // the infinity, and nan-symbol. If either matches, the function assumes, that
  // a match has been found, and expects the following input characters to match
  // the remaining characters of the special-value symbol.
  // This means that the following restrictions apply to special-value symbols:
  //  - they must not start with signs ('+', or '-'),
  //  - they must not have the same first character.
  //  - they must not start with digits.
  //
  // If the separator character is not kNoSeparator, then that specific
  // character is ignored when in between two valid digits of the significant.
  // It is not allowed to appear in the exponent.
  // It is not allowed to lead or trail the number.
  // It is not allowed to appear twice next to each other.
  //
  // Examples:
  //  flags = ALLOW_HEX | ALLOW_TRAILING_JUNK,
  //  empty_string_value = 0.0,
  //  junk_string_value = NaN,
  //  infinity_symbol = "infinity",
  //  nan_symbol = "nan":
  //    StringToDouble("0x1234") -> 4660.0.
  //    StringToDouble("0x1234K") -> 4660.0.
  //    StringToDouble("") -> 0.0  // empty_string_value.
  //    StringToDouble(" ") -> NaN  // junk_string_value.
  //    StringToDouble(" 1") -> NaN  // junk_string_value.
  //    StringToDouble("0x") -> NaN  // junk_string_value.
  //    StringToDouble("-123.45") -> -123.45.
  //    StringToDouble("--123.45") -> NaN  // junk_string_value.
  //    StringToDouble("123e45") -> 123e45.
  //    StringToDouble("123E45") -> 123e45.
  //    StringToDouble("123e+45") -> 123e45.
  //    StringToDouble("123E-45") -> 123e-45.
  //    StringToDouble("123e") -> 123.0  // trailing junk ignored.
  //    StringToDouble("123e-") -> 123.0  // trailing junk ignored.
  //    StringToDouble("+NaN") -> NaN  // NaN string literal.
  //    StringToDouble("-infinity") -> -inf.  // infinity literal.
  //    StringToDouble("Infinity") -> NaN  // junk_string_value.
  //
  //  flags = ALLOW_OCTAL | ALLOW_LEADING_SPACES,
  //  empty_string_value = 0.0,
  //  junk_string_value = NaN,
  //  infinity_symbol = NULL,
  //  nan_symbol = NULL:
  //    StringToDouble("0x1234") -> NaN  // junk_string_value.
  //    StringToDouble("01234") -> 668.0.
  //    StringToDouble("") -> 0.0  // empty_string_value.
  //    StringToDouble(" ") -> 0.0  // empty_string_value.
  //    StringToDouble(" 1") -> 1.0
  //    StringToDouble("0x") -> NaN  // junk_string_value.
  //    StringToDouble("0123e45") -> NaN  // junk_string_value.
  //    StringToDouble("01239E45") -> 1239e45.
  //    StringToDouble("-infinity") -> NaN  // junk_string_value.
  //    StringToDouble("NaN") -> NaN  // junk_string_value.
  //
  //  flags = NO_FLAGS,
  //  separator = ' ':
  //    StringToDouble("1 2 3 4") -> 1234.0
  //    StringToDouble("1  2") -> NaN // junk_string_value
  //    StringToDouble("1 000 000.0") -> 1000000.0
  //    StringToDouble("1.000 000") -> 1.0
  //    StringToDouble("1.0e1 000") -> NaN // junk_string_value
  StringToDoubleConverter(int flags,
                          double empty_string_value,
                          double junk_string_value,
                          const char* infinity_symbol,
                          const char* nan_symbol,
                          uc16 separator = kNoSeparator)
      : flags_(flags),
        empty_string_value_(empty_string_value),
        junk_string_value_(junk_string_value),
        infinity_symbol_(infinity_symbol),
        nan_symbol_(nan_symbol),
        separator_(separator) {
  }

  // Performs the conversion.
  // The output parameter 'processed_characters_count' is set to the number
  // of characters that have been processed to read the number.
  // Spaces than are processed with ALLOW_{LEADING|TRAILING}_SPACES are included
  // in the 'processed_characters_count'. Trailing junk is never included.
  double StringToDouble(const char* buffer,
                        int length,
                        int* processed_characters_count) const;

  // Same as StringToDouble above but for 16 bit characters.
  double StringToDouble(const uc16* buffer,
                        int length,
                        int* processed_characters_count) const;

  // Same as StringToDouble but reads a float.
  // Note that this is not equivalent to static_cast<float>(StringToDouble(...))
  // due to potential double-rounding.
  float StringToFloat(const char* buffer,
                      int length,
                      int* processed_characters_count) const;

  // Same as StringToFloat above but for 16 bit characters.
  float StringToFloat(const uc16* buffer,
                      int length,
                      int* processed_characters_count) const;

 private:
  const int flags_;
  const double empty_string_value_;
  const double junk_string_value_;
  const char* const infinity_symbol_;
  const char* const nan_symbol_;
  const uc16 separator_;

  template <class Iterator>
  double StringToIeee(Iterator start_pointer,
                      int length,
                      bool read_as_double,
                      int* processed_characters_count) const;

  DOUBLE_CONVERSION_DISALLOW_IMPLICIT_CONSTRUCTORS(StringToDoubleConverter);
};

}  // namespace double_conversion

// ICU PATCH: Close ICU namespace
U_NAMESPACE_END

#endif  // DOUBLE_CONVERSION_STRING_TO_DOUBLE_H_
#endif // ICU PATCH: close #if !UCONFIG_NO_FORMATTING
