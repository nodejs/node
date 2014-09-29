// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stdarg.h>
#include <cmath>

#include "src/v8.h"

#include "src/assert-scope.h"
#include "src/conversions-inl.h"
#include "src/conversions.h"
#include "src/dtoa.h"
#include "src/factory.h"
#include "src/list-inl.h"
#include "src/strtod.h"
#include "src/utils.h"

#ifndef _STLP_VENDOR_CSTD
// STLPort doesn't import fpclassify into the std namespace.
using std::fpclassify;
#endif

namespace v8 {
namespace internal {


namespace {

// C++-style iterator adaptor for StringCharacterStream
// (unlike C++ iterators the end-marker has different type).
class StringCharacterStreamIterator {
 public:
  class EndMarker {};

  explicit StringCharacterStreamIterator(StringCharacterStream* stream);

  uint16_t operator*() const;
  void operator++();
  bool operator==(EndMarker const&) const { return end_; }
  bool operator!=(EndMarker const& m) const { return !end_; }

 private:
  StringCharacterStream* const stream_;
  uint16_t current_;
  bool end_;
};


StringCharacterStreamIterator::StringCharacterStreamIterator(
    StringCharacterStream* stream) : stream_(stream) {
  ++(*this);
}

uint16_t StringCharacterStreamIterator::operator*() const {
  return current_;
}


void StringCharacterStreamIterator::operator++() {
  end_ = !stream_->HasMore();
  if (!end_) {
    current_ = stream_->GetNext();
  }
}
}  // End anonymous namespace.


double StringToDouble(UnicodeCache* unicode_cache,
                      const char* str, int flags, double empty_string_val) {
  // We cast to const uint8_t* here to avoid instantiating the
  // InternalStringToDouble() template for const char* as well.
  const uint8_t* start = reinterpret_cast<const uint8_t*>(str);
  const uint8_t* end = start + StrLength(str);
  return InternalStringToDouble(unicode_cache, start, end, flags,
                                empty_string_val);
}


double StringToDouble(UnicodeCache* unicode_cache,
                      Vector<const uint8_t> str,
                      int flags,
                      double empty_string_val) {
  // We cast to const uint8_t* here to avoid instantiating the
  // InternalStringToDouble() template for const char* as well.
  const uint8_t* start = reinterpret_cast<const uint8_t*>(str.start());
  const uint8_t* end = start + str.length();
  return InternalStringToDouble(unicode_cache, start, end, flags,
                                empty_string_val);
}


double StringToDouble(UnicodeCache* unicode_cache,
                      Vector<const uc16> str,
                      int flags,
                      double empty_string_val) {
  const uc16* end = str.start() + str.length();
  return InternalStringToDouble(unicode_cache, str.start(), end, flags,
                                empty_string_val);
}


// Converts a string into an integer.
double StringToInt(UnicodeCache* unicode_cache,
                   Vector<const uint8_t> vector,
                   int radix) {
  return InternalStringToInt(
      unicode_cache, vector.start(), vector.start() + vector.length(), radix);
}


double StringToInt(UnicodeCache* unicode_cache,
                   Vector<const uc16> vector,
                   int radix) {
  return InternalStringToInt(
      unicode_cache, vector.start(), vector.start() + vector.length(), radix);
}


const char* DoubleToCString(double v, Vector<char> buffer) {
  switch (fpclassify(v)) {
    case FP_NAN: return "NaN";
    case FP_INFINITE: return (v < 0.0 ? "-Infinity" : "Infinity");
    case FP_ZERO: return "0";
    default: {
      SimpleStringBuilder builder(buffer.start(), buffer.length());
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
        builder.AddDecimalInteger(exponent);
      }
    return builder.Finalize();
    }
  }
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
  DCHECK(f >= 0);
  DCHECK(f <= kMaxDigitsAfterPoint);

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
  SimpleStringBuilder rep_builder(rep_length + 1);
  rep_builder.AddPadding('0', zero_prefix_length);
  rep_builder.AddString(decimal_rep);
  rep_builder.AddPadding('0', zero_postfix_length);
  char* rep = rep_builder.Finalize();

  // Create the result string by appending a minus and putting in a
  // decimal point if needed.
  unsigned result_size = decimal_point + f + 2;
  SimpleStringBuilder builder(result_size + 1);
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
  SimpleStringBuilder builder(result_size + 1);

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
  builder.AddDecimalInteger(exponent);
  return builder.Finalize();
}


char* DoubleToExponentialCString(double value, int f) {
  const int kMaxDigitsAfterPoint = 20;
  // f might be -1 to signal that f was undefined in JavaScript.
  DCHECK(f >= -1 && f <= kMaxDigitsAfterPoint);

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
  DCHECK(kBase10MaximalLength <= kMaxDigitsAfterPoint + 1);
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
  DCHECK(decimal_rep_length > 0);
  DCHECK(decimal_rep_length <= f + 1);

  int exponent = decimal_point - 1;
  char* result =
      CreateExponentialRepresentation(decimal_rep, exponent, negative, f+1);

  return result;
}


char* DoubleToPrecisionCString(double value, int p) {
  const int kMinimalDigits = 1;
  const int kMaximalDigits = 21;
  DCHECK(p >= kMinimalDigits && p <= kMaximalDigits);
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
  DCHECK(decimal_rep_length <= p);

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
    SimpleStringBuilder builder(result_size + 1);
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
  DCHECK(radix >= 2 && radix <= 36);

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
  double integer_part = std::floor(value);
  double decimal_part = value - integer_part;

  // Convert the integer part starting from the back.  Always generate
  // at least one digit.
  int integer_pos = kBufferSize - 2;
  do {
    double remainder = std::fmod(integer_part, radix);
    integer_buffer[integer_pos--] = chars[static_cast<int>(remainder)];
    integer_part -= remainder;
    integer_part /= radix;
  } while (integer_part >= 1.0);
  // Sanity check.
  DCHECK(integer_pos > 0);
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
        chars[static_cast<int>(std::floor(decimal_part))];
    decimal_part -= std::floor(decimal_part);
  }
  decimal_buffer[decimal_pos] = '\0';

  // Compute the result size.
  int integer_part_size = kBufferSize - 2 - integer_pos;
  // Make room for zero termination.
  unsigned result_size = integer_part_size + decimal_pos;
  // If the number has a decimal part, leave room for the period.
  if (decimal_pos > 0) result_size++;
  // Allocate result and fill in the parts.
  SimpleStringBuilder builder(result_size + 1);
  builder.AddSubstring(integer_buffer + integer_pos + 1, integer_part_size);
  if (decimal_pos > 0) builder.AddCharacter('.');
  builder.AddSubstring(decimal_buffer, decimal_pos);
  return builder.Finalize();
}


double StringToDouble(UnicodeCache* unicode_cache,
                      String* string,
                      int flags,
                      double empty_string_val) {
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = string->GetFlatContent();
  // ECMA-262 section 15.1.2.3, empty string is NaN
  if (flat.IsAscii()) {
    return StringToDouble(
        unicode_cache, flat.ToOneByteVector(), flags, empty_string_val);
  } else {
    return StringToDouble(
        unicode_cache, flat.ToUC16Vector(), flags, empty_string_val);
  }
}


} }  // namespace v8::internal
