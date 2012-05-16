// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_CONVERSIONS_INL_H_
#define V8_CONVERSIONS_INL_H_

#include <limits.h>        // Required for INT_MAX etc.
#include <math.h>
#include <float.h>         // Required for DBL_MAX and on Win32 for finite()
#include <stdarg.h>
#include "globals.h"       // Required for V8_INFINITY

// ----------------------------------------------------------------------------
// Extra POSIX/ANSI functions for Win32/MSVC.

#include "conversions.h"
#include "double.h"
#include "platform.h"
#include "scanner.h"
#include "strtod.h"

namespace v8 {
namespace internal {

inline double JunkStringValue() {
  return BitCast<double, uint64_t>(kQuietNaNMask);
}


// The fast double-to-unsigned-int conversion routine does not guarantee
// rounding towards zero, or any reasonable value if the argument is larger
// than what fits in an unsigned 32-bit integer.
inline unsigned int FastD2UI(double x) {
  // There is no unsigned version of lrint, so there is no fast path
  // in this function as there is in FastD2I. Using lrint doesn't work
  // for values of 2^31 and above.

  // Convert "small enough" doubles to uint32_t by fixing the 32
  // least significant non-fractional bits in the low 32 bits of the
  // double, and reading them from there.
  const double k2Pow52 = 4503599627370496.0;
  bool negative = x < 0;
  if (negative) {
    x = -x;
  }
  if (x < k2Pow52) {
    x += k2Pow52;
    uint32_t result;
    Address mantissa_ptr = reinterpret_cast<Address>(&x);
    // Copy least significant 32 bits of mantissa.
    memcpy(&result, mantissa_ptr, sizeof(result));
    return negative ? ~result + 1 : result;
  }
  // Large number (outside uint32 range), Infinity or NaN.
  return 0x80000000u;  // Return integer indefinite.
}


inline double DoubleToInteger(double x) {
  if (isnan(x)) return 0;
  if (!isfinite(x) || x == 0) return x;
  return (x >= 0) ? floor(x) : ceil(x);
}


int32_t DoubleToInt32(double x) {
  int32_t i = FastD2I(x);
  if (FastI2D(i) == x) return i;
  Double d(x);
  int exponent = d.Exponent();
  if (exponent < 0) {
    if (exponent <= -Double::kSignificandSize) return 0;
    return d.Sign() * static_cast<int32_t>(d.Significand() >> -exponent);
  } else {
    if (exponent > 31) return 0;
    return d.Sign() * static_cast<int32_t>(d.Significand() << exponent);
  }
}


template <class Iterator, class EndMark>
bool SubStringEquals(Iterator* current,
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


// Returns true if a nonspace character has been found and false if the
// end was been reached before finding a nonspace character.
template <class Iterator, class EndMark>
inline bool AdvanceToNonspace(UnicodeCache* unicode_cache,
                              Iterator* current,
                              EndMark end) {
  while (*current != end) {
    if (!unicode_cache->IsWhiteSpace(**current)) return true;
    ++*current;
  }
  return false;
}


// Parsing integers with radix 2, 4, 8, 16, 32. Assumes current != end.
template <int radix_log_2, class Iterator, class EndMark>
double InternalStringToIntDouble(UnicodeCache* unicode_cache,
                                 Iterator current,
                                 EndMark end,
                                 bool negative,
                                 bool allow_trailing_junk) {
  ASSERT(current != end);

  // Skip leading 0s.
  while (*current == '0') {
    ++current;
    if (current == end) return SignedZero(negative);
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
      if (allow_trailing_junk ||
          !AdvanceToNonspace(unicode_cache, &current, end)) {
        break;
      } else {
        return JunkStringValue();
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

      if (!allow_trailing_junk &&
          AdvanceToNonspace(unicode_cache, &current, end)) {
        return JunkStringValue();
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
    if (negative) {
      if (number == 0) return -0.0;
      number = -number;
    }
    return static_cast<double>(number);
  }

  ASSERT(number != 0);
  return ldexp(static_cast<double>(negative ? -number : number), exponent);
}


template <class Iterator, class EndMark>
double InternalStringToInt(UnicodeCache* unicode_cache,
                           Iterator current,
                           EndMark end,
                           int radix) {
  const bool allow_trailing_junk = true;
  const double empty_string_val = JunkStringValue();

  if (!AdvanceToNonspace(unicode_cache, &current, end)) {
    return empty_string_val;
  }

  bool negative = false;
  bool leading_zero = false;

  if (*current == '+') {
    // Ignore leading sign; skip following spaces.
    ++current;
    if (current == end) {
      return JunkStringValue();
    }
  } else if (*current == '-') {
    ++current;
    if (current == end) {
      return JunkStringValue();
    }
    negative = true;
  }

  if (radix == 0) {
    // Radix detection.
    if (*current == '0') {
      ++current;
      if (current == end) return SignedZero(negative);
      if (*current == 'x' || *current == 'X') {
        radix = 16;
        ++current;
        if (current == end) return JunkStringValue();
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
      if (current == end) return SignedZero(negative);
      if (*current == 'x' || *current == 'X') {
        ++current;
        if (current == end) return JunkStringValue();
      } else {
        leading_zero = true;
      }
    }
  }

  if (radix < 2 || radix > 36) return JunkStringValue();

  // Skip leading zeros.
  while (*current == '0') {
    leading_zero = true;
    ++current;
    if (current == end) return SignedZero(negative);
  }

  if (!leading_zero && !isDigit(*current, radix)) {
    return JunkStringValue();
  }

  if (IsPowerOf2(radix)) {
    switch (radix) {
      case 2:
        return InternalStringToIntDouble<1>(
            unicode_cache, current, end, negative, allow_trailing_junk);
      case 4:
        return InternalStringToIntDouble<2>(
            unicode_cache, current, end, negative, allow_trailing_junk);
      case 8:
        return InternalStringToIntDouble<3>(
            unicode_cache, current, end, negative, allow_trailing_junk);

      case 16:
        return InternalStringToIntDouble<4>(
            unicode_cache, current, end, negative, allow_trailing_junk);

      case 32:
        return InternalStringToIntDouble<5>(
            unicode_cache, current, end, negative, allow_trailing_junk);
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

    if (!allow_trailing_junk &&
        AdvanceToNonspace(unicode_cache, &current, end)) {
      return JunkStringValue();
    }

    ASSERT(buffer_pos < kBufferSize);
    buffer[buffer_pos] = '\0';
    Vector<const char> buffer_vector(buffer, buffer_pos);
    return negative ? -Strtod(buffer_vector, 0) : Strtod(buffer_vector, 0);
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

  if (!allow_trailing_junk &&
      AdvanceToNonspace(unicode_cache, &current, end)) {
    return JunkStringValue();
  }

  return negative ? -v : v;
}


// Converts a string to a double value. Assumes the Iterator supports
// the following operations:
// 1. current == end (other ops are not allowed), current != end.
// 2. *current - gets the current character in the sequence.
// 3. ++current (advances the position).
template <class Iterator, class EndMark>
double InternalStringToDouble(UnicodeCache* unicode_cache,
                              Iterator current,
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
  if (!AdvanceToNonspace(unicode_cache, &current, end)) {
    return empty_string_val;
  }

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

  bool negative = false;

  if (*current == '+') {
    // Ignore leading sign.
    ++current;
    if (current == end) return JunkStringValue();
  } else if (*current == '-') {
    ++current;
    if (current == end) return JunkStringValue();
    negative = true;
  }

  static const char kInfinitySymbol[] = "Infinity";
  if (*current == kInfinitySymbol[0]) {
    if (!SubStringEquals(&current, end, kInfinitySymbol)) {
      return JunkStringValue();
    }

    if (!allow_trailing_junk &&
        AdvanceToNonspace(unicode_cache, &current, end)) {
      return JunkStringValue();
    }

    ASSERT(buffer_pos == 0);
    return negative ? -V8_INFINITY : V8_INFINITY;
  }

  bool leading_zero = false;
  if (*current == '0') {
    ++current;
    if (current == end) return SignedZero(negative);

    leading_zero = true;

    // It could be hexadecimal value.
    if ((flags & ALLOW_HEX) && (*current == 'x' || *current == 'X')) {
      ++current;
      if (current == end || !isDigit(*current, 16)) {
        return JunkStringValue();  // "0x".
      }

      return InternalStringToIntDouble<4>(unicode_cache,
                                          current,
                                          end,
                                          negative,
                                          allow_trailing_junk);
    }

    // Ignore leading zeros in the integer part.
    while (*current == '0') {
      ++current;
      if (current == end) return SignedZero(negative);
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
    if (octal && !allow_trailing_junk) return JunkStringValue();
    if (octal) goto parsing_done;

    ++current;
    if (current == end) {
      if (significant_digits == 0 && !leading_zero) {
        return JunkStringValue();
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
        if (current == end) return SignedZero(negative);
        exponent--;  // Move this 0 into the exponent.
      }
    }

    // There is a fractional part.  We don't emit a '.', but adjust the exponent
    // instead.
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
    return JunkStringValue();
  }

  // Parse exponential part.
  if (*current == 'e' || *current == 'E') {
    if (octal) return JunkStringValue();
    ++current;
    if (current == end) {
      if (allow_trailing_junk) {
        goto parsing_done;
      } else {
        return JunkStringValue();
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
          return JunkStringValue();
        }
      }
    }

    if (current == end || *current < '0' || *current > '9') {
      if (allow_trailing_junk) {
        goto parsing_done;
      } else {
        return JunkStringValue();
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

  if (!allow_trailing_junk &&
      AdvanceToNonspace(unicode_cache, &current, end)) {
    return JunkStringValue();
  }

  parsing_done:
  exponent += insignificant_digits;

  if (octal) {
    return InternalStringToIntDouble<3>(unicode_cache,
                                        buffer,
                                        buffer + buffer_pos,
                                        negative,
                                        allow_trailing_junk);
  }

  if (nonzero_digit_dropped) {
    buffer[buffer_pos++] = '1';
    exponent--;
  }

  ASSERT(buffer_pos < kBufferSize);
  buffer[buffer_pos] = '\0';

  double converted = Strtod(Vector<const char>(buffer, buffer_pos), exponent);
  return negative ? -converted : converted;
}

} }  // namespace v8::internal

#endif  // V8_CONVERSIONS_INL_H_
