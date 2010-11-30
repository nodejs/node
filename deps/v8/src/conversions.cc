// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include <stdarg.h>
#include <limits.h>

#include "v8.h"

#include "conversions-inl.h"
#include "dtoa.h"
#include "factory.h"
#include "scanner-base.h"
#include "strtod.h"

namespace v8 {
namespace internal {

namespace {

// C++-style iterator adaptor for StringInputBuffer
// (unlike C++ iterators the end-marker has different type).
class StringInputBufferIterator {
 public:
  class EndMarker {};

  explicit StringInputBufferIterator(StringInputBuffer* buffer);

  int operator*() const;
  void operator++();
  bool operator==(EndMarker const&) const { return end_; }
  bool operator!=(EndMarker const& m) const { return !end_; }

 private:
  StringInputBuffer* const buffer_;
  int current_;
  bool end_;
};


StringInputBufferIterator::StringInputBufferIterator(
    StringInputBuffer* buffer) : buffer_(buffer) {
  ++(*this);
}

int StringInputBufferIterator::operator*() const {
  return current_;
}


void StringInputBufferIterator::operator++() {
  end_ = !buffer_->has_more();
  if (!end_) {
    current_ = buffer_->GetNext();
  }
}
}


template <class Iterator, class EndMark>
static bool SubStringEquals(Iterator* current,
                            EndMark end,
                            const char* substring) {
  ASSERT(**current == *substring);
  for (substring++; *substring != '\0'; substring++) {
    ++*current;
    if (*current == end || **current != *substring) return false;
  }
  ++*current;
  return true;
}


// Maximum number of significant digits in decimal representation.
// The longest possible double in decimal representation is
// (2^53 - 1) * 2 ^ -1074 that is (2 ^ 53 - 1) * 5 ^ 1074 / 10 ^ 1074
// (768 digits). If we parse a number whose first digits are equal to a
// mean of 2 adjacent doubles (that could have up to 769 digits) the result
// must be rounded to the bigger one unless the tail consists of zeros, so
// we don't need to preserve all the digits.
const int kMaxSignificantDigits = 772;


static const double JUNK_STRING_VALUE = OS::nan_value();


// Returns true if a nonspace found and false if the end has reached.
template <class Iterator, class EndMark>
static inline bool AdvanceToNonspace(Iterator* current, EndMark end) {
  while (*current != end) {
    if (!ScannerConstants::kIsWhiteSpace.get(**current)) return true;
    ++*current;
  }
  return false;
}


static bool isDigit(int x, int radix) {
  return (x >= '0' && x <= '9' && x < '0' + radix)
      || (radix > 10 && x >= 'a' && x < 'a' + radix - 10)
      || (radix > 10 && x >= 'A' && x < 'A' + radix - 10);
}


static double SignedZero(bool sign) {
  return sign ? -0.0 : 0.0;
}


// Parsing integers with radix 2, 4, 8, 16, 32. Assumes current != end.
template <int radix_log_2, class Iterator, class EndMark>
static double InternalStringToIntDouble(Iterator current,
                                        EndMark end,
                                        bool sign,
                                        bool allow_trailing_junk) {
  ASSERT(current != end);

  // Skip leading 0s.
  while (*current == '0') {
    ++current;
    if (current == end) return SignedZero(sign);
  }

  int64_t number = 0;
  int exponent = 0;
  const int radix = (1 << radix_log_2);

  do {
    int digit;
    if (*current >= '0' && *current <= '9' && *current < '0' + radix) {
      digit = static_cast<char>(*current) - '0';
    } else if (radix > 10 && *current >= 'a' && *current < 'a' + radix - 10) {
      digit = static_cast<char>(*current) - 'a' + 10;
    } else if (radix > 10 && *current >= 'A' && *current < 'A' + radix - 10) {
      digit = static_cast<char>(*current) - 'A' + 10;
    } else {
      if (allow_trailing_junk || !AdvanceToNonspace(&current, end)) {
        break;
      } else {
        return JUNK_STRING_VALUE;
      }
    }

    number = number * radix + digit;
    int overflow = static_cast<int>(number >> 53);
    if (overflow != 0) {
      // Overflow occurred. Need to determine which direction to round the
      // result.
      int overflow_bits_count = 1;
      while (overflow > 1) {
        overflow_bits_count++;
        overflow >>= 1;
      }

      int dropped_bits_mask = ((1 << overflow_bits_count) - 1);
      int dropped_bits = static_cast<int>(number) & dropped_bits_mask;
      number >>= overflow_bits_count;
      exponent = overflow_bits_count;

      bool zero_tail = true;
      while (true) {
        ++current;
        if (current == end || !isDigit(*current, radix)) break;
        zero_tail = zero_tail && *current == '0';
        exponent += radix_log_2;
      }

      if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
        return JUNK_STRING_VALUE;
      }

      int middle_value = (1 << (overflow_bits_count - 1));
      if (dropped_bits > middle_value) {
        number++;  // Rounding up.
      } else if (dropped_bits == middle_value) {
        // Rounding to even to consistency with decimals: half-way case rounds
        // up if significant part is odd and down otherwise.
        if ((number & 1) != 0 || !zero_tail) {
          number++;  // Rounding up.
        }
      }

      // Rounding up may cause overflow.
      if ((number & ((int64_t)1 << 53)) != 0) {
        exponent++;
        number >>= 1;
      }
      break;
    }
    ++current;
  } while (current != end);

  ASSERT(number < ((int64_t)1 << 53));
  ASSERT(static_cast<int64_t>(static_cast<double>(number)) == number);

  if (exponent == 0) {
    if (sign) {
      if (number == 0) return -0.0;
      number = -number;
    }
    return static_cast<double>(number);
  }

  ASSERT(number != 0);
  // The double could be constructed faster from number (mantissa), exponent
  // and sign. Assuming it's a rare case more simple code is used.
  return static_cast<double>(sign ? -number : number) * pow(2.0, exponent);
}


template <class Iterator, class EndMark>
static double InternalStringToInt(Iterator current, EndMark end, int radix) {
  const bool allow_trailing_junk = true;
  const double empty_string_val = JUNK_STRING_VALUE;

  if (!AdvanceToNonspace(&current, end)) return empty_string_val;

  bool sign = false;
  bool leading_zero = false;

  if (*current == '+') {
    // Ignore leading sign; skip following spaces.
    ++current;
    if (!AdvanceToNonspace(&current, end)) return JUNK_STRING_VALUE;
  } else if (*current == '-') {
    ++current;
    if (!AdvanceToNonspace(&current, end)) return JUNK_STRING_VALUE;
    sign = true;
  }

  if (radix == 0) {
    // Radix detection.
    if (*current == '0') {
      ++current;
      if (current == end) return SignedZero(sign);
      if (*current == 'x' || *current == 'X') {
        radix = 16;
        ++current;
        if (current == end) return JUNK_STRING_VALUE;
      } else {
        radix = 8;
        leading_zero = true;
      }
    } else {
      radix = 10;
    }
  } else if (radix == 16) {
    if (*current == '0') {
      // Allow "0x" prefix.
      ++current;
      if (current == end) return SignedZero(sign);
      if (*current == 'x' || *current == 'X') {
        ++current;
        if (current == end) return JUNK_STRING_VALUE;
      } else {
        leading_zero = true;
      }
    }
  }

  if (radix < 2 || radix > 36) return JUNK_STRING_VALUE;

  // Skip leading zeros.
  while (*current == '0') {
    leading_zero = true;
    ++current;
    if (current == end) return SignedZero(sign);
  }

  if (!leading_zero && !isDigit(*current, radix)) {
    return JUNK_STRING_VALUE;
  }

  if (IsPowerOf2(radix)) {
    switch (radix) {
      case 2:
        return InternalStringToIntDouble<1>(
                   current, end, sign, allow_trailing_junk);
      case 4:
        return InternalStringToIntDouble<2>(
                   current, end, sign, allow_trailing_junk);
      case 8:
        return InternalStringToIntDouble<3>(
                   current, end, sign, allow_trailing_junk);

      case 16:
        return InternalStringToIntDouble<4>(
                   current, end, sign, allow_trailing_junk);

      case 32:
        return InternalStringToIntDouble<5>(
                   current, end, sign, allow_trailing_junk);
      default:
        UNREACHABLE();
    }
  }

  if (radix == 10) {
    // Parsing with strtod.
    const int kMaxSignificantDigits = 309;  // Doubles are less than 1.8e308.
    // The buffer may contain up to kMaxSignificantDigits + 1 digits and a zero
    // end.
    const int kBufferSize = kMaxSignificantDigits + 2;
    char buffer[kBufferSize];
    int buffer_pos = 0;
    while (*current >= '0' && *current <= '9') {
      if (buffer_pos <= kMaxSignificantDigits) {
        // If the number has more than kMaxSignificantDigits it will be parsed
        // as infinity.
        ASSERT(buffer_pos < kBufferSize);
        buffer[buffer_pos++] = static_cast<char>(*current);
      }
      ++current;
      if (current == end) break;
    }

    if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
      return JUNK_STRING_VALUE;
    }

    ASSERT(buffer_pos < kBufferSize);
    buffer[buffer_pos] = '\0';
    Vector<const char> buffer_vector(buffer, buffer_pos);
    return sign ? -Strtod(buffer_vector, 0) : Strtod(buffer_vector, 0);
  }

  // The following code causes accumulating rounding error for numbers greater
  // than ~2^56. It's explicitly allowed in the spec: "if R is not 2, 4, 8, 10,
  // 16, or 32, then mathInt may be an implementation-dependent approximation to
  // the mathematical integer value" (15.1.2.2).

  int lim_0 = '0' + (radix < 10 ? radix : 10);
  int lim_a = 'a' + (radix - 10);
  int lim_A = 'A' + (radix - 10);

  // NOTE: The code for computing the value may seem a bit complex at
  // first glance. It is structured to use 32-bit multiply-and-add
  // loops as long as possible to avoid loosing precision.

  double v = 0.0;
  bool done = false;
  do {
    // Parse the longest part of the string starting at index j
    // possible while keeping the multiplier, and thus the part
    // itself, within 32 bits.
    unsigned int part = 0, multiplier = 1;
    while (true) {
      int d;
      if (*current >= '0' && *current < lim_0) {
        d = *current - '0';
      } else if (*current >= 'a' && *current < lim_a) {
        d = *current - 'a' + 10;
      } else if (*current >= 'A' && *current < lim_A) {
        d = *current - 'A' + 10;
      } else {
        done = true;
        break;
      }

      // Update the value of the part as long as the multiplier fits
      // in 32 bits. When we can't guarantee that the next iteration
      // will not overflow the multiplier, we stop parsing the part
      // by leaving the loop.
      const unsigned int kMaximumMultiplier = 0xffffffffU / 36;
      uint32_t m = multiplier * radix;
      if (m > kMaximumMultiplier) break;
      part = part * radix + d;
      multiplier = m;
      ASSERT(multiplier > part);

      ++current;
      if (current == end) {
        done = true;
        break;
      }
    }

    // Update the value and skip the part in the string.
    v = v * multiplier + part;
  } while (!done);

  if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
    return JUNK_STRING_VALUE;
  }

  return sign ? -v : v;
}


// Converts a string to a double value. Assumes the Iterator supports
// the following operations:
// 1. current == end (other ops are not allowed), current != end.
// 2. *current - gets the current character in the sequence.
// 3. ++current (advances the position).
template <class Iterator, class EndMark>
static double InternalStringToDouble(Iterator current,
                                     EndMark end,
                                     int flags,
                                     double empty_string_val) {
  // To make sure that iterator dereferencing is valid the following
  // convention is used:
  // 1. Each '++current' statement is followed by check for equality to 'end'.
  // 2. If AdvanceToNonspace returned false then current == end.
  // 3. If 'current' becomes be equal to 'end' the function returns or goes to
  // 'parsing_done'.
  // 4. 'current' is not dereferenced after the 'parsing_done' label.
  // 5. Code before 'parsing_done' may rely on 'current != end'.
  if (!AdvanceToNonspace(&current, end)) return empty_string_val;

  const bool allow_trailing_junk = (flags & ALLOW_TRAILING_JUNK) != 0;

  // The longest form of simplified number is: "-<significant digits>'.1eXXX\0".
  const int kBufferSize = kMaxSignificantDigits + 10;
  char buffer[kBufferSize];  // NOLINT: size is known at compile time.
  int buffer_pos = 0;

  // Exponent will be adjusted if insignificant digits of the integer part
  // or insignificant leading zeros of the fractional part are dropped.
  int exponent = 0;
  int significant_digits = 0;
  int insignificant_digits = 0;
  bool nonzero_digit_dropped = false;
  bool fractional_part = false;

  bool sign = false;

  if (*current == '+') {
    // Ignore leading sign.
    ++current;
    if (current == end) return JUNK_STRING_VALUE;
  } else if (*current == '-') {
    ++current;
    if (current == end) return JUNK_STRING_VALUE;
    sign = true;
  }

  static const char kInfinitySymbol[] = "Infinity";
  if (*current == kInfinitySymbol[0]) {
    if (!SubStringEquals(&current, end, kInfinitySymbol)) {
      return JUNK_STRING_VALUE;
    }

    if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
      return JUNK_STRING_VALUE;
    }

    ASSERT(buffer_pos == 0);
    return sign ? -V8_INFINITY : V8_INFINITY;
  }

  bool leading_zero = false;
  if (*current == '0') {
    ++current;
    if (current == end) return SignedZero(sign);

    leading_zero = true;

    // It could be hexadecimal value.
    if ((flags & ALLOW_HEX) && (*current == 'x' || *current == 'X')) {
      ++current;
      if (current == end || !isDigit(*current, 16)) {
        return JUNK_STRING_VALUE;  // "0x".
      }

      return InternalStringToIntDouble<4>(current,
                                          end,
                                          sign,
                                          allow_trailing_junk);
    }

    // Ignore leading zeros in the integer part.
    while (*current == '0') {
      ++current;
      if (current == end) return SignedZero(sign);
    }
  }

  bool octal = leading_zero && (flags & ALLOW_OCTALS) != 0;

  // Copy significant digits of the integer part (if any) to the buffer.
  while (*current >= '0' && *current <= '9') {
    if (significant_digits < kMaxSignificantDigits) {
      ASSERT(buffer_pos < kBufferSize);
      buffer[buffer_pos++] = static_cast<char>(*current);
      significant_digits++;
      // Will later check if it's an octal in the buffer.
    } else {
      insignificant_digits++;  // Move the digit into the exponential part.
      nonzero_digit_dropped = nonzero_digit_dropped || *current != '0';
    }
    octal = octal && *current < '8';
    ++current;
    if (current == end) goto parsing_done;
  }

  if (significant_digits == 0) {
    octal = false;
  }

  if (*current == '.') {
    if (octal && !allow_trailing_junk) return JUNK_STRING_VALUE;
    if (octal) goto parsing_done;

    ++current;
    if (current == end) {
      if (significant_digits == 0 && !leading_zero) {
        return JUNK_STRING_VALUE;
      } else {
        goto parsing_done;
      }
    }

    if (significant_digits == 0) {
      // octal = false;
      // Integer part consists of 0 or is absent. Significant digits start after
      // leading zeros (if any).
      while (*current == '0') {
        ++current;
        if (current == end) return SignedZero(sign);
        exponent--;  // Move this 0 into the exponent.
      }
    }

    // We don't emit a '.', but adjust the exponent instead.
    fractional_part = true;

    // There is a fractional part.
    while (*current >= '0' && *current <= '9') {
      if (significant_digits < kMaxSignificantDigits) {
        ASSERT(buffer_pos < kBufferSize);
        buffer[buffer_pos++] = static_cast<char>(*current);
        significant_digits++;
        exponent--;
      } else {
        // Ignore insignificant digits in the fractional part.
        nonzero_digit_dropped = nonzero_digit_dropped || *current != '0';
      }
      ++current;
      if (current == end) goto parsing_done;
    }
  }

  if (!leading_zero && exponent == 0 && significant_digits == 0) {
    // If leading_zeros is true then the string contains zeros.
    // If exponent < 0 then string was [+-]\.0*...
    // If significant_digits != 0 the string is not equal to 0.
    // Otherwise there are no digits in the string.
    return JUNK_STRING_VALUE;
  }

  // Parse exponential part.
  if (*current == 'e' || *current == 'E') {
    if (octal) return JUNK_STRING_VALUE;
    ++current;
    if (current == end) {
      if (allow_trailing_junk) {
        goto parsing_done;
      } else {
        return JUNK_STRING_VALUE;
      }
    }
    char sign = '+';
    if (*current == '+' || *current == '-') {
      sign = static_cast<char>(*current);
      ++current;
      if (current == end) {
        if (allow_trailing_junk) {
          goto parsing_done;
        } else {
          return JUNK_STRING_VALUE;
        }
      }
    }

    if (current == end || *current < '0' || *current > '9') {
      if (allow_trailing_junk) {
        goto parsing_done;
      } else {
        return JUNK_STRING_VALUE;
      }
    }

    const int max_exponent = INT_MAX / 2;
    ASSERT(-max_exponent / 2 <= exponent && exponent <= max_exponent / 2);
    int num = 0;
    do {
      // Check overflow.
      int digit = *current - '0';
      if (num >= max_exponent / 10
          && !(num == max_exponent / 10 && digit <= max_exponent % 10)) {
        num = max_exponent;
      } else {
        num = num * 10 + digit;
      }
      ++current;
    } while (current != end && *current >= '0' && *current <= '9');

    exponent += (sign == '-' ? -num : num);
  }

  if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
    return JUNK_STRING_VALUE;
  }

  parsing_done:
  exponent += insignificant_digits;

  if (octal) {
    return InternalStringToIntDouble<3>(buffer,
                                        buffer + buffer_pos,
                                        sign,
                                        allow_trailing_junk);
  }

  if (nonzero_digit_dropped) {
    buffer[buffer_pos++] = '1';
    exponent--;
  }

  ASSERT(buffer_pos < kBufferSize);
  buffer[buffer_pos] = '\0';

  double converted = Strtod(Vector<const char>(buffer, buffer_pos), exponent);
  return sign ? -converted : converted;
}


double StringToDouble(String* str, int flags, double empty_string_val) {
  StringShape shape(str);
  if (shape.IsSequentialAscii()) {
    const char* begin = SeqAsciiString::cast(str)->GetChars();
    const char* end = begin + str->length();
    return InternalStringToDouble(begin, end, flags, empty_string_val);
  } else if (shape.IsSequentialTwoByte()) {
    const uc16* begin = SeqTwoByteString::cast(str)->GetChars();
    const uc16* end = begin + str->length();
    return InternalStringToDouble(begin, end, flags, empty_string_val);
  } else {
    StringInputBuffer buffer(str);
    return InternalStringToDouble(StringInputBufferIterator(&buffer),
                                  StringInputBufferIterator::EndMarker(),
                                  flags,
                                  empty_string_val);
  }
}


double StringToInt(String* str, int radix) {
  StringShape shape(str);
  if (shape.IsSequentialAscii()) {
    const char* begin = SeqAsciiString::cast(str)->GetChars();
    const char* end = begin + str->length();
    return InternalStringToInt(begin, end, radix);
  } else if (shape.IsSequentialTwoByte()) {
    const uc16* begin = SeqTwoByteString::cast(str)->GetChars();
    const uc16* end = begin + str->length();
    return InternalStringToInt(begin, end, radix);
  } else {
    StringInputBuffer buffer(str);
    return InternalStringToInt(StringInputBufferIterator(&buffer),
                               StringInputBufferIterator::EndMarker(),
                               radix);
  }
}


double StringToDouble(const char* str, int flags, double empty_string_val) {
  const char* end = str + StrLength(str);
  return InternalStringToDouble(str, end, flags, empty_string_val);
}


double StringToDouble(Vector<const char> str,
                      int flags,
                      double empty_string_val) {
  const char* end = str.start() + str.length();
  return InternalStringToDouble(str.start(), end, flags, empty_string_val);
}


const char* DoubleToCString(double v, Vector<char> buffer) {
  StringBuilder builder(buffer.start(), buffer.length());

  switch (fpclassify(v)) {
    case FP_NAN:
      builder.AddString("NaN");
      break;

    case FP_INFINITE:
      if (v < 0.0) {
        builder.AddString("-Infinity");
      } else {
        builder.AddString("Infinity");
      }
      break;

    case FP_ZERO:
      builder.AddCharacter('0');
      break;

    default: {
      int decimal_point;
      int sign;
      const int kV8DtoaBufferCapacity = kBase10MaximalLength + 1;
      char decimal_rep[kV8DtoaBufferCapacity];
      int length;

      DoubleToAscii(v, DTOA_SHORTEST, 0,
                    Vector<char>(decimal_rep, kV8DtoaBufferCapacity),
                    &sign, &length, &decimal_point);

      if (sign) builder.AddCharacter('-');

      if (length <= decimal_point && decimal_point <= 21) {
        // ECMA-262 section 9.8.1 step 6.
        builder.AddString(decimal_rep);
        builder.AddPadding('0', decimal_point - length);

      } else if (0 < decimal_point && decimal_point <= 21) {
        // ECMA-262 section 9.8.1 step 7.
        builder.AddSubstring(decimal_rep, decimal_point);
        builder.AddCharacter('.');
        builder.AddString(decimal_rep + decimal_point);

      } else if (decimal_point <= 0 && decimal_point > -6) {
        // ECMA-262 section 9.8.1 step 8.
        builder.AddString("0.");
        builder.AddPadding('0', -decimal_point);
        builder.AddString(decimal_rep);

      } else {
        // ECMA-262 section 9.8.1 step 9 and 10 combined.
        builder.AddCharacter(decimal_rep[0]);
        if (length != 1) {
          builder.AddCharacter('.');
          builder.AddString(decimal_rep + 1);
        }
        builder.AddCharacter('e');
        builder.AddCharacter((decimal_point >= 0) ? '+' : '-');
        int exponent = decimal_point - 1;
        if (exponent < 0) exponent = -exponent;
        builder.AddFormatted("%d", exponent);
      }
    }
  }
  return builder.Finalize();
}


const char* IntToCString(int n, Vector<char> buffer) {
  bool negative = false;
  if (n < 0) {
    // We must not negate the most negative int.
    if (n == kMinInt) return DoubleToCString(n, buffer);
    negative = true;
    n = -n;
  }
  // Build the string backwards from the least significant digit.
  int i = buffer.length();
  buffer[--i] = '\0';
  do {
    buffer[--i] = '0' + (n % 10);
    n /= 10;
  } while (n);
  if (negative) buffer[--i] = '-';
  return buffer.start() + i;
}


char* DoubleToFixedCString(double value, int f) {
  const int kMaxDigitsBeforePoint = 21;
  const double kFirstNonFixed = 1e21;
  const int kMaxDigitsAfterPoint = 20;
  ASSERT(f >= 0);
  ASSERT(f <= kMaxDigitsAfterPoint);

  bool negative = false;
  double abs_value = value;
  if (value < 0) {
    abs_value = -value;
    negative = true;
  }

  // If abs_value has more than kMaxDigitsBeforePoint digits before the point
  // use the non-fixed conversion routine.
  if (abs_value >= kFirstNonFixed) {
    char arr[100];
    Vector<char> buffer(arr, ARRAY_SIZE(arr));
    return StrDup(DoubleToCString(value, buffer));
  }

  // Find a sufficiently precise decimal representation of n.
  int decimal_point;
  int sign;
  // Add space for the '\0' byte.
  const int kDecimalRepCapacity =
      kMaxDigitsBeforePoint + kMaxDigitsAfterPoint + 1;
  char decimal_rep[kDecimalRepCapacity];
  int decimal_rep_length;
  DoubleToAscii(value, DTOA_FIXED, f,
                Vector<char>(decimal_rep, kDecimalRepCapacity),
                &sign, &decimal_rep_length, &decimal_point);

  // Create a representation that is padded with zeros if needed.
  int zero_prefix_length = 0;
  int zero_postfix_length = 0;

  if (decimal_point <= 0) {
    zero_prefix_length = -decimal_point + 1;
    decimal_point = 1;
  }

  if (zero_prefix_length + decimal_rep_length < decimal_point + f) {
    zero_postfix_length = decimal_point + f - decimal_rep_length -
                          zero_prefix_length;
  }

  unsigned rep_length =
      zero_prefix_length + decimal_rep_length + zero_postfix_length;
  StringBuilder rep_builder(rep_length + 1);
  rep_builder.AddPadding('0', zero_prefix_length);
  rep_builder.AddString(decimal_rep);
  rep_builder.AddPadding('0', zero_postfix_length);
  char* rep = rep_builder.Finalize();

  // Create the result string by appending a minus and putting in a
  // decimal point if needed.
  unsigned result_size = decimal_point + f + 2;
  StringBuilder builder(result_size + 1);
  if (negative) builder.AddCharacter('-');
  builder.AddSubstring(rep, decimal_point);
  if (f > 0) {
    builder.AddCharacter('.');
    builder.AddSubstring(rep + decimal_point, f);
  }
  DeleteArray(rep);
  return builder.Finalize();
}


static char* CreateExponentialRepresentation(char* decimal_rep,
                                             int exponent,
                                             bool negative,
                                             int significant_digits) {
  bool negative_exponent = false;
  if (exponent < 0) {
    negative_exponent = true;
    exponent = -exponent;
  }

  // Leave room in the result for appending a minus, for a period, the
  // letter 'e', a minus or a plus depending on the exponent, and a
  // three digit exponent.
  unsigned result_size = significant_digits + 7;
  StringBuilder builder(result_size + 1);

  if (negative) builder.AddCharacter('-');
  builder.AddCharacter(decimal_rep[0]);
  if (significant_digits != 1) {
    builder.AddCharacter('.');
    builder.AddString(decimal_rep + 1);
    int rep_length = StrLength(decimal_rep);
    builder.AddPadding('0', significant_digits - rep_length);
  }

  builder.AddCharacter('e');
  builder.AddCharacter(negative_exponent ? '-' : '+');
  builder.AddFormatted("%d", exponent);
  return builder.Finalize();
}



char* DoubleToExponentialCString(double value, int f) {
  const int kMaxDigitsAfterPoint = 20;
  // f might be -1 to signal that f was undefined in JavaScript.
  ASSERT(f >= -1 && f <= kMaxDigitsAfterPoint);

  bool negative = false;
  if (value < 0) {
    value = -value;
    negative = true;
  }

  // Find a sufficiently precise decimal representation of n.
  int decimal_point;
  int sign;
  // f corresponds to the digits after the point. There is always one digit
  // before the point. The number of requested_digits equals hence f + 1.
  // And we have to add one character for the null-terminator.
  const int kV8DtoaBufferCapacity = kMaxDigitsAfterPoint + 1 + 1;
  // Make sure that the buffer is big enough, even if we fall back to the
  // shortest representation (which happens when f equals -1).
  ASSERT(kBase10MaximalLength <= kMaxDigitsAfterPoint + 1);
  char decimal_rep[kV8DtoaBufferCapacity];
  int decimal_rep_length;

  if (f == -1) {
    DoubleToAscii(value, DTOA_SHORTEST, 0,
                  Vector<char>(decimal_rep, kV8DtoaBufferCapacity),
                  &sign, &decimal_rep_length, &decimal_point);
    f = decimal_rep_length - 1;
  } else {
    DoubleToAscii(value, DTOA_PRECISION, f + 1,
                  Vector<char>(decimal_rep, kV8DtoaBufferCapacity),
                  &sign, &decimal_rep_length, &decimal_point);
  }
  ASSERT(decimal_rep_length > 0);
  ASSERT(decimal_rep_length <= f + 1);

  int exponent = decimal_point - 1;
  char* result =
      CreateExponentialRepresentation(decimal_rep, exponent, negative, f+1);

  return result;
}


char* DoubleToPrecisionCString(double value, int p) {
  const int kMinimalDigits = 1;
  const int kMaximalDigits = 21;
  ASSERT(p >= kMinimalDigits && p <= kMaximalDigits);
  USE(kMinimalDigits);

  bool negative = false;
  if (value < 0) {
    value = -value;
    negative = true;
  }

  // Find a sufficiently precise decimal representation of n.
  int decimal_point;
  int sign;
  // Add one for the terminating null character.
  const int kV8DtoaBufferCapacity = kMaximalDigits + 1;
  char decimal_rep[kV8DtoaBufferCapacity];
  int decimal_rep_length;

  DoubleToAscii(value, DTOA_PRECISION, p,
                Vector<char>(decimal_rep, kV8DtoaBufferCapacity),
                &sign, &decimal_rep_length, &decimal_point);
  ASSERT(decimal_rep_length <= p);

  int exponent = decimal_point - 1;

  char* result = NULL;

  if (exponent < -6 || exponent >= p) {
    result =
        CreateExponentialRepresentation(decimal_rep, exponent, negative, p);
  } else {
    // Use fixed notation.
    //
    // Leave room in the result for appending a minus, a period and in
    // the case where decimal_point is not positive for a zero in
    // front of the period.
    unsigned result_size = (decimal_point <= 0)
        ? -decimal_point + p + 3
        : p + 2;
    StringBuilder builder(result_size + 1);
    if (negative) builder.AddCharacter('-');
    if (decimal_point <= 0) {
      builder.AddString("0.");
      builder.AddPadding('0', -decimal_point);
      builder.AddString(decimal_rep);
      builder.AddPadding('0', p - decimal_rep_length);
    } else {
      const int m = Min(decimal_rep_length, decimal_point);
      builder.AddSubstring(decimal_rep, m);
      builder.AddPadding('0', decimal_point - decimal_rep_length);
      if (decimal_point < p) {
        builder.AddCharacter('.');
        const int extra = negative ? 2 : 1;
        if (decimal_rep_length > decimal_point) {
          const int len = StrLength(decimal_rep + decimal_point);
          const int n = Min(len, p - (builder.position() - extra));
          builder.AddSubstring(decimal_rep + decimal_point, n);
        }
        builder.AddPadding('0', extra + (p - builder.position()));
      }
    }
    result = builder.Finalize();
  }

  return result;
}


char* DoubleToRadixCString(double value, int radix) {
  ASSERT(radix >= 2 && radix <= 36);

  // Character array used for conversion.
  static const char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  // Buffer for the integer part of the result. 1024 chars is enough
  // for max integer value in radix 2.  We need room for a sign too.
  static const int kBufferSize = 1100;
  char integer_buffer[kBufferSize];
  integer_buffer[kBufferSize - 1] = '\0';

  // Buffer for the decimal part of the result.  We only generate up
  // to kBufferSize - 1 chars for the decimal part.
  char decimal_buffer[kBufferSize];
  decimal_buffer[kBufferSize - 1] = '\0';

  // Make sure the value is positive.
  bool is_negative = value < 0.0;
  if (is_negative) value = -value;

  // Get the integer part and the decimal part.
  double integer_part = floor(value);
  double decimal_part = value - integer_part;

  // Convert the integer part starting from the back.  Always generate
  // at least one digit.
  int integer_pos = kBufferSize - 2;
  do {
    integer_buffer[integer_pos--] =
        chars[static_cast<int>(modulo(integer_part, radix))];
    integer_part /= radix;
  } while (integer_part >= 1.0);
  // Sanity check.
  ASSERT(integer_pos > 0);
  // Add sign if needed.
  if (is_negative) integer_buffer[integer_pos--] = '-';

  // Convert the decimal part.  Repeatedly multiply by the radix to
  // generate the next char.  Never generate more than kBufferSize - 1
  // chars.
  //
  // TODO(1093998): We will often generate a full decimal_buffer of
  // chars because hitting zero will often not happen.  The right
  // solution would be to continue until the string representation can
  // be read back and yield the original value.  To implement this
  // efficiently, we probably have to modify dtoa.
  int decimal_pos = 0;
  while ((decimal_part > 0.0) && (decimal_pos < kBufferSize - 1)) {
    decimal_part *= radix;
    decimal_buffer[decimal_pos++] =
        chars[static_cast<int>(floor(decimal_part))];
    decimal_part -= floor(decimal_part);
  }
  decimal_buffer[decimal_pos] = '\0';

  // Compute the result size.
  int integer_part_size = kBufferSize - 2 - integer_pos;
  // Make room for zero termination.
  unsigned result_size = integer_part_size + decimal_pos;
  // If the number has a decimal part, leave room for the period.
  if (decimal_pos > 0) result_size++;
  // Allocate result and fill in the parts.
  StringBuilder builder(result_size + 1);
  builder.AddSubstring(integer_buffer + integer_pos + 1, integer_part_size);
  if (decimal_pos > 0) builder.AddCharacter('.');
  builder.AddSubstring(decimal_buffer, decimal_pos);
  return builder.Finalize();
}


} }  // namespace v8::internal
