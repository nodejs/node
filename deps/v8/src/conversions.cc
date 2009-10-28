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

#include "v8.h"

#include "conversions-inl.h"
#include "factory.h"
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
  ASSERT(index >= 0 && index < static_cast<int>(strlen(str)));
  return str[index];
}


static inline int GetChar(String* str, int index) {
  return str->Get(index);
}


static inline int GetLength(const char* str) {
  return strlen(str);
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


static inline void ReleaseCString(const char* original, const char* str) {
}


static inline void ReleaseCString(String* original, const char* str) {
  DeleteArray(const_cast<char *>(str));
}


static inline bool IsSpace(const char* str, int index) {
  ASSERT(index >= 0 && index < static_cast<int>(strlen(str)));
  return Scanner::kIsWhiteSpace.get(str[index]);
}


static inline bool IsSpace(String* str, int index) {
  return Scanner::kIsWhiteSpace.get(str->Get(index));
}


static inline bool SubStringEquals(const char* str,
                                   int index,
                                   const char* other) {
  return strncmp(str + index, other, strlen(other)) != 0;
}


static inline bool SubStringEquals(String* str, int index, const char* other) {
  HandleScope scope;
  int str_length = str->length();
  int other_length = strlen(other);
  int end = index + other_length < str_length ?
            index + other_length :
            str_length;
  Handle<String> slice =
      Factory::NewStringSlice(Handle<String>(str), index, end);
  return slice->IsEqualTo(Vector<const char>(other, other_length));
}


// Check if a string should be parsed as an octal number.  The string
// can be either a char* or a String*.
template<class S>
static bool ShouldParseOctal(S* s, int i) {
  int index = i;
  int len = GetLength(s);
  if (index < len && GetChar(s, index) != '0') return false;

  // If the first real character (following '0') is not an octal
  // digit, bail out early. This also takes care of numbers of the
  // forms 0.xxx and 0exxx by not allowing the first 0 to be
  // interpreted as an octal.
  index++;
  if (index < len) {
    int d = GetChar(s, index) - '0';
    if (d < 0 || d > 7) return false;
  } else {
    return false;
  }

  // Traverse all digits (including the first). If there is an octal
  // prefix which is not a part of a longer decimal prefix, we return
  // true. Otherwise, false is returned.
  while (index < len) {
    int d = GetChar(s, index++) - '0';
    if (d == 8 || d == 9) return false;
    if (d <  0 || d >  7) return true;
  }
  return true;
}


extern "C" double gay_strtod(const char* s00, const char** se);


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


// Convert a string to a double value.  The string can be either a
// char* or a String*.
template<class S>
static double InternalStringToDouble(S* str,
                                     int flags,
                                     double empty_string_val) {
  double result = 0.0;
  int index = 0;

  int len = GetLength(str);

  // Skip leading spaces.
  while ((index < len) && IsSpace(str, index)) index++;

  // Is the string empty?
  if (index >= len) return empty_string_val;

  // Get the first character.
  uint16_t first = GetChar(str, index);

  // Numbers can only start with '-', '+', '.', 'I' (Infinity), or a digit.
  if (first != '-' && first != '+' && first != '.' && first != 'I' &&
      (first > '9' || first < '0')) {
    return JUNK_STRING_VALUE;
  }

  // Compute sign of result based on first character.
  int sign = 1;
  if (first == '-') {
    sign = -1;
    index++;
    // String only containing a '-' are junk chars.
    if (index == len) return JUNK_STRING_VALUE;
  }

  // do we have a hex number?
  // (since the string is 0-terminated, it's ok to look one char beyond the end)
  if ((flags & ALLOW_HEX) != 0 &&
      (index + 1) < len &&
      GetChar(str, index) == '0' &&
      (GetChar(str, index + 1) == 'x' || GetChar(str, index + 1) == 'X')) {
    index += 2;
    index = StringToInt(str, index, 16, &result);
  } else if ((flags & ALLOW_OCTALS) != 0 && ShouldParseOctal(str, index)) {
    // NOTE: We optimistically try to parse the number as an octal (if
    // we're allowed to), even though this is not as dictated by
    // ECMA-262. The reason for doing this is compatibility with IE and
    // Firefox.
    index = StringToInt(str, index, 8, &result);
  } else {
    const char* cstr = GetCString(str, index);
    const char* end;
    // Optimistically parse the number and then, if that fails,
    // check if it might have been {+,-,}Infinity.
    result = gay_strtod(cstr, &end);
    ReleaseCString(str, cstr);
    if (result != 0.0 || end != cstr) {
      // It appears that strtod worked
      index += end - cstr;
    } else {
      // Check for {+,-,}Infinity
      bool is_negative = (GetChar(str, index) == '-');
      if (GetChar(str, index) == '+' || GetChar(str, index) == '-')
        index++;
      if (!SubStringEquals(str, index, "Infinity"))
        return JUNK_STRING_VALUE;
      result = is_negative ? -V8_INFINITY : V8_INFINITY;
      index += 8;
    }
  }

  if ((flags & ALLOW_TRAILING_JUNK) == 0) {
    // skip trailing spaces
    while ((index < len) && IsSpace(str, index)) index++;
    // string ending with junk?
    if (index < len) return JUNK_STRING_VALUE;
  }

  return sign * result;
}


double StringToDouble(String* str, int flags, double empty_string_val) {
  return InternalStringToDouble(str, flags, empty_string_val);
}


double StringToDouble(const char* str, int flags, double empty_string_val) {
  return InternalStringToDouble(str, flags, empty_string_val);
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

      char* decimal_rep = dtoa(v, 0, 0, &decimal_point, &sign, NULL);
      int length = strlen(decimal_rep);

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

      freedtoa(decimal_rep);
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
  int decimal_rep_length = strlen(decimal_rep);

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
    builder.AddPadding('0', significant_digits - strlen(decimal_rep));
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
    f = strlen(decimal_rep) - 1;
  } else {
    decimal_rep = dtoa(value, 2, f + 1, &decimal_point, &sign, NULL);
  }
  int decimal_rep_length = strlen(decimal_rep);
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
  int decimal_rep_length = strlen(decimal_rep);
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
          const int len = strlen(decimal_rep + decimal_point);
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
