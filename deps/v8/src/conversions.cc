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
#include "factory.h"
#include "fast-dtoa.h"
#include "scanner.h"

namespace v8 {
namespace internal {

int HexValue(uc32 c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  return -1;
}


// Provide a common interface to getting a character at a certain
// index from a char* or a String object.
static inline int GetChar(const char* str, int index) {
  ASSERT(index >= 0 && index < StrLength(str));
  return str[index];
}


static inline int GetChar(String* str, int index) {
  return str->Get(index);
}


static inline int GetLength(const char* str) {
  return StrLength(str);
}


static inline int GetLength(String* str) {
  return str->length();
}


static inline const char* GetCString(const char* str, int index) {
  return str + index;
}


static inline const char* GetCString(String* str, int index) {
  int length = str->length();
  char* result = NewArray<char>(length + 1);
  for (int i = index; i < length; i++) {
    uc16 c = str->Get(i);
    if (c <= 127) {
      result[i - index] = static_cast<char>(c);
    } else {
      result[i - index] = 127;  // Force number parsing to fail.
    }
  }
  result[length - index] = '\0';
  return result;
}


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


static inline void ReleaseCString(const char* original, const char* str) {
}


static inline void ReleaseCString(String* original, const char* str) {
  DeleteArray(const_cast<char *>(str));
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


extern "C" double gay_strtod(const char* s00, const char** se);

// Maximum number of significant digits in decimal representation.
// The longest possible double in decimal representation is
// (2^53 - 1) * 2 ^ -1074 that is (2 ^ 53 - 1) * 5 ^ 1074 / 10 ^ 1074
// (768 digits). If we parse a number whose first digits are equal to a
// mean of 2 adjacent doubles (that could have up to 769 digits) the result
// must be rounded to the bigger one unless the tail consists of zeros, so
// we don't need to preserve all the digits.
const int kMaxSignificantDigits = 772;

// Parse an int from a string starting a given index and in a given
// radix.  The string can be either a char* or a String*.
template <class S>
static int InternalStringToInt(S* s, int i, int radix, double* value) {
  int len = GetLength(s);

  // Setup limits for computing the value.
  ASSERT(2 <= radix && radix <= 36);
  int lim_0 = '0' + (radix < 10 ? radix : 10);
  int lim_a = 'a' + (radix - 10);
  int lim_A = 'A' + (radix - 10);

  // NOTE: The code for computing the value may seem a bit complex at
  // first glance. It is structured to use 32-bit multiply-and-add
  // loops as long as possible to avoid loosing precision.

  double v = 0.0;
  int j;
  for (j = i; j < len;) {
    // Parse the longest part of the string starting at index j
    // possible while keeping the multiplier, and thus the part
    // itself, within 32 bits.
    uint32_t part = 0, multiplier = 1;
    int k;
    for (k = j; k < len; k++) {
      int c = GetChar(s, k);
      if (c >= '0' && c < lim_0) {
        c = c - '0';
      } else if (c >= 'a' && c < lim_a) {
        c = c - 'a' + 10;
      } else if (c >= 'A' && c < lim_A) {
        c = c - 'A' + 10;
      } else {
        break;
      }

      // Update the value of the part as long as the multiplier fits
      // in 32 bits. When we can't guarantee that the next iteration
      // will not overflow the multiplier, we stop parsing the part
      // by leaving the loop.
      static const uint32_t kMaximumMultiplier = 0xffffffffU / 36;
      uint32_t m = multiplier * radix;
      if (m > kMaximumMultiplier) break;
      part = part * radix + c;
      multiplier = m;
      ASSERT(multiplier > part);
    }

    // Compute the number of part digits. If no digits were parsed;
    // we're done parsing the entire string.
    int digits = k - j;
    if (digits == 0) break;

    // Update the value and skip the part in the string.
    ASSERT(multiplier ==
           pow(static_cast<double>(radix), static_cast<double>(digits)));
    v = v * multiplier + part;
    j = k;
  }

  // If the resulting value is larger than 2^53 the value does not fit
  // in the mantissa of the double and there is a loss of precision.
  // When the value is larger than 2^53 the rounding depends on the
  // code generation.  If the code generator spills the double value
  // it uses 64 bits and if it does not it uses 80 bits.
  //
  // If there is a potential for overflow we resort to strtod for
  // radix 10 numbers to get higher precision.  For numbers in another
  // radix we live with the loss of precision.
  static const double kPreciseConversionLimit = 9007199254740992.0;
  if (radix == 10 && v > kPreciseConversionLimit) {
    const char* cstr = GetCString(s, i);
    const char* end;
    v = gay_strtod(cstr, &end);
    ReleaseCString(s, cstr);
  }

  *value = v;
  return j;
}


int StringToInt(String* str, int index, int radix, double* value) {
  return InternalStringToInt(str, index, radix, value);
}


int StringToInt(const char* str, int index, int radix, double* value) {
  return InternalStringToInt(const_cast<char*>(str), index, radix, value);
}


static const double JUNK_STRING_VALUE = OS::nan_value();


// Returns true if a nonspace found and false if the end has reached.
template <class Iterator, class EndMark>
static inline bool AdvanceToNonspace(Iterator* current, EndMark end) {
  while (*current != end) {
    if (!Scanner::kIsWhiteSpace.get(**current)) return true;
    ++*current;
  }
  return false;
}


template <class Iterator, class EndMark>
static double InternalHexadecimalStringToDouble(Iterator current,
                                                EndMark end,
                                                char* buffer,
                                                bool allow_trailing_junk) {
  ASSERT(current != end);

  const int max_hex_significant_digits = 52 / 4 + 2;
  // We reuse the buffer of InternalStringToDouble. Since hexadecimal
  // numbers may have much less digits than decimal the buffer won't overflow.
  ASSERT(max_hex_significant_digits < kMaxSignificantDigits);

  int significant_digits = 0;
  int insignificant_digits = 0;
  bool leading_zero = false;
  // A double has a 53bit significand (once the hidden bit has been added).
  // Halfway cases thus have at most 54bits. Therefore 54/4 + 1 digits are
  // sufficient to represent halfway cases. By adding another digit we can keep
  // track of dropped digits.
  int buffer_pos = 0;
  bool nonzero_digit_dropped = false;

  // Skip leading 0s.
  while (*current == '0') {
    leading_zero = true;
    ++current;
    if (current == end) return 0;
  }

  int begin_pos = buffer_pos;
  while ((*current >= '0' && *current <= '9')
         || (*current >= 'a' && *current <= 'f')
         || (*current >= 'A' && *current <= 'F')) {
    if (significant_digits <= max_hex_significant_digits) {
      buffer[buffer_pos++] = static_cast<char>(*current);
      significant_digits++;
    } else {
      insignificant_digits++;
      nonzero_digit_dropped = nonzero_digit_dropped || *current != '0';
    }
    ++current;
    if (current == end) break;
  }

  if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
    return JUNK_STRING_VALUE;
  }

  if (significant_digits == 0) {
    return leading_zero ? 0 : JUNK_STRING_VALUE;
  }

  if (nonzero_digit_dropped) {
    ASSERT(insignificant_digits > 0);
    insignificant_digits--;
    buffer[buffer_pos++] = '1';
  }

  buffer[buffer_pos] = '\0';

  double result;
  StringToInt(buffer, begin_pos, 16, &result);
  if (insignificant_digits > 0) {
    // Multiplying by a power of 2 doesn't cause a loss of precision.
    result *= pow(16.0, insignificant_digits);
  }
  return result;
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

  double signed_zero = 0.0;

  if (*current == '+') {
    // Ignore leading sign; skip following spaces.
    ++current;
    if (!AdvanceToNonspace(&current, end)) return JUNK_STRING_VALUE;
  } else if (*current == '-') {
    buffer[buffer_pos++] = '-';
    ++current;
    if (!AdvanceToNonspace(&current, end)) return JUNK_STRING_VALUE;
    signed_zero = -0.0;
  }

  static const char kInfinitySymbol[] = "Infinity";
  if (*current == kInfinitySymbol[0]) {
    if (!SubStringEquals(&current, end, kInfinitySymbol)) {
      return JUNK_STRING_VALUE;
    }

    if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
      return JUNK_STRING_VALUE;
    }

    ASSERT(buffer_pos == 0 || buffer[0] == '-');
    return buffer_pos > 0 ? -V8_INFINITY : V8_INFINITY;
  }

  bool leading_zero = false;
  if (*current == '0') {
    ++current;
    if (current == end) return signed_zero;

    leading_zero = true;

    // It could be hexadecimal value.
    if ((flags & ALLOW_HEX) && (*current == 'x' || *current == 'X')) {
      ++current;
      if (current == end) return JUNK_STRING_VALUE;  // "0x".

      double result = InternalHexadecimalStringToDouble(current,
                                                        end,
                                                        buffer + buffer_pos,
                                                        allow_trailing_junk);
      return (buffer_pos > 0 && buffer[0] == '-') ? -result : result;
    }

    // Ignore leading zeros in the integer part.
    while (*current == '0') {
      ++current;
      if (current == end) return signed_zero;
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
    ASSERT(buffer_pos < kBufferSize);
    buffer[buffer_pos++] = '.';
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
        if (current == end) return signed_zero;
        exponent--;  // Move this 0 into the exponent.
      }
    }

    // There is the fractional part.
    while (*current >= '0' && *current <= '9') {
      if (significant_digits < kMaxSignificantDigits) {
        ASSERT(buffer_pos < kBufferSize);
        buffer[buffer_pos++] = static_cast<char>(*current);
        significant_digits++;
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
    buffer[buffer_pos] = '\0';
    // ALLOW_OCTALS is set and there is no '8' or '9' in insignificant
    // digits. Check significant digits now.
    char sign = '+';
    const char* s = buffer;
    if (*s == '-' || *s == '+') sign = *s++;

    double result;
    s += StringToInt(s, 0, 8, &result);
    if (!allow_trailing_junk && *s != '\0') return JUNK_STRING_VALUE;

    if (sign == '-') result = -result;
    if (insignificant_digits > 0) {
      result *= pow(8.0, insignificant_digits);
    }
    return result;
  }

  if (nonzero_digit_dropped) {
    if (insignificant_digits) buffer[buffer_pos++] = '.';
    buffer[buffer_pos++] = '1';
  }

  if (exponent != 0) {
    ASSERT(buffer_pos < kBufferSize);
    buffer[buffer_pos++] = 'e';
    if (exponent < 0) {
      ASSERT(buffer_pos < kBufferSize);
      buffer[buffer_pos++] = '-';
      exponent = -exponent;
    }
    if (exponent > 999) exponent = 999;  // Result will be Infinity or 0 or -0.

    const int exp_digits = 3;
    for (int i = 0; i < exp_digits; i++) {
      buffer[buffer_pos + exp_digits - 1 - i] = '0' + exponent % 10;
      exponent /= 10;
    }
    ASSERT(exponent == 0);
    buffer_pos += exp_digits;
  }

  ASSERT(buffer_pos < kBufferSize);
  buffer[buffer_pos] = '\0';

  return gay_strtod(buffer, NULL);
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


double StringToDouble(const char* str, int flags, double empty_string_val) {
  const char* end = str + StrLength(str);

  return InternalStringToDouble(str, end, flags, empty_string_val);
}


extern "C" char* dtoa(double d, int mode, int ndigits,
                      int* decpt, int* sign, char** rve);

extern "C" void freedtoa(char* s);

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

      char* decimal_rep;
      bool used_gay_dtoa = false;
      char fast_dtoa_buffer[kFastDtoaMaximalLength + 1];
      int length;
      if (FastDtoa(v, fast_dtoa_buffer, &sign, &length, &decimal_point)) {
        decimal_rep = fast_dtoa_buffer;
      } else {
        decimal_rep = dtoa(v, 0, 0, &decimal_point, &sign, NULL);
        used_gay_dtoa = true;
        length = StrLength(decimal_rep);
      }

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

      if (used_gay_dtoa) freedtoa(decimal_rep);
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
  ASSERT(f >= 0);

  bool negative = false;
  double abs_value = value;
  if (value < 0) {
    abs_value = -value;
    negative = true;
  }

  if (abs_value >= 1e21) {
    char arr[100];
    Vector<char> buffer(arr, ARRAY_SIZE(arr));
    return StrDup(DoubleToCString(value, buffer));
  }

  // Find a sufficiently precise decimal representation of n.
  int decimal_point;
  int sign;
  char* decimal_rep = dtoa(abs_value, 3, f, &decimal_point, &sign, NULL);
  int decimal_rep_length = StrLength(decimal_rep);

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
  freedtoa(decimal_rep);

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
  // f might be -1 to signal that f was undefined in JavaScript.
  ASSERT(f >= -1 && f <= 20);

  bool negative = false;
  if (value < 0) {
    value = -value;
    negative = true;
  }

  // Find a sufficiently precise decimal representation of n.
  int decimal_point;
  int sign;
  char* decimal_rep = NULL;
  if (f == -1) {
    decimal_rep = dtoa(value, 0, 0, &decimal_point, &sign, NULL);
    f = StrLength(decimal_rep) - 1;
  } else {
    decimal_rep = dtoa(value, 2, f + 1, &decimal_point, &sign, NULL);
  }
  int decimal_rep_length = StrLength(decimal_rep);
  ASSERT(decimal_rep_length > 0);
  ASSERT(decimal_rep_length <= f + 1);
  USE(decimal_rep_length);

  int exponent = decimal_point - 1;
  char* result =
      CreateExponentialRepresentation(decimal_rep, exponent, negative, f+1);

  freedtoa(decimal_rep);

  return result;
}


char* DoubleToPrecisionCString(double value, int p) {
  ASSERT(p >= 1 && p <= 21);

  bool negative = false;
  if (value < 0) {
    value = -value;
    negative = true;
  }

  // Find a sufficiently precise decimal representation of n.
  int decimal_point;
  int sign;
  char* decimal_rep = dtoa(value, 2, p, &decimal_point, &sign, NULL);
  int decimal_rep_length = StrLength(decimal_rep);
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

  freedtoa(decimal_rep);
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
