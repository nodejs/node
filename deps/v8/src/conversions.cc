// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/conversions.h"

#include <limits.h>
#include <stdarg.h>
#include <cmath>

#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/char-predicates-inl.h"
#include "src/dtoa.h"
#include "src/handles.h"
#include "src/heap/factory.h"
#include "src/objects-inl.h"
#include "src/objects/bigint.h"
#include "src/strtod.h"
#include "src/utils.h"

#if defined(_STLP_VENDOR_CSTD)
// STLPort doesn't import fpclassify into the std namespace.
#define FPCLASSIFY_NAMESPACE
#else
#define FPCLASSIFY_NAMESPACE std
#endif

namespace v8 {
namespace internal {

inline double JunkStringValue() {
  return bit_cast<double, uint64_t>(kQuietNaNMask);
}

inline double SignedZero(bool negative) {
  return negative ? uint64_to_double(Double::kSignMask) : 0.0;
}

inline bool isDigit(int x, int radix) {
  return (x >= '0' && x <= '9' && x < '0' + radix) ||
         (radix > 10 && x >= 'a' && x < 'a' + radix - 10) ||
         (radix > 10 && x >= 'A' && x < 'A' + radix - 10);
}

inline bool isBinaryDigit(int x) { return x == '0' || x == '1'; }

template <class Iterator, class EndMark>
bool SubStringEquals(Iterator* current, EndMark end, const char* substring) {
  DCHECK(**current == *substring);
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
inline bool AdvanceToNonspace(Iterator* current, EndMark end) {
  while (*current != end) {
    if (!IsWhiteSpaceOrLineTerminator(**current)) return true;
    ++*current;
  }
  return false;
}

// Parsing integers with radix 2, 4, 8, 16, 32. Assumes current != end.
template <int radix_log_2, class Iterator, class EndMark>
double InternalStringToIntDouble(Iterator current, EndMark end, bool negative,
                                 bool allow_trailing_junk) {
  DCHECK(current != end);

  // Skip leading 0s.
  while (*current == '0') {
    ++current;
    if (current == end) return SignedZero(negative);
  }

  int64_t number = 0;
  int exponent = 0;
  const int radix = (1 << radix_log_2);

  int lim_0 = '0' + (radix < 10 ? radix : 10);
  int lim_a = 'a' + (radix - 10);
  int lim_A = 'A' + (radix - 10);

  do {
    int digit;
    if (*current >= '0' && *current < lim_0) {
      digit = static_cast<char>(*current) - '0';
    } else if (*current >= 'a' && *current < lim_a) {
      digit = static_cast<char>(*current) - 'a' + 10;
    } else if (*current >= 'A' && *current < lim_A) {
      digit = static_cast<char>(*current) - 'A' + 10;
    } else {
      if (allow_trailing_junk || !AdvanceToNonspace(&current, end)) {
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

      if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
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
      if ((number & (static_cast<int64_t>(1) << 53)) != 0) {
        exponent++;
        number >>= 1;
      }
      break;
    }
    ++current;
  } while (current != end);

  DCHECK(number < ((int64_t)1 << 53));
  DCHECK(static_cast<int64_t>(static_cast<double>(number)) == number);

  if (exponent == 0) {
    if (negative) {
      if (number == 0) return -0.0;
      number = -number;
    }
    return static_cast<double>(number);
  }

  DCHECK_NE(number, 0);
  return std::ldexp(static_cast<double>(negative ? -number : number), exponent);
}

// ES6 18.2.5 parseInt(string, radix) (with NumberParseIntHelper subclass);
// and BigInt parsing cases from https://tc39.github.io/proposal-bigint/
// (with StringToBigIntHelper subclass).
class StringToIntHelper {
 public:
  StringToIntHelper(Isolate* isolate, Handle<String> subject, int radix)
      : isolate_(isolate), subject_(subject), radix_(radix) {
    DCHECK(subject->IsFlat());
  }

  // Used for the StringToBigInt operation.
  StringToIntHelper(Isolate* isolate, Handle<String> subject)
      : isolate_(isolate), subject_(subject) {
    DCHECK(subject->IsFlat());
  }

  // Used for parsing BigInt literals, where the input is a Zone-allocated
  // buffer of one-byte digits, along with an optional radix prefix.
  StringToIntHelper(Isolate* isolate, const uint8_t* subject, int length)
      : isolate_(isolate), raw_one_byte_subject_(subject), length_(length) {}
  virtual ~StringToIntHelper() = default;

 protected:
  // Subclasses must implement these:
  virtual void AllocateResult() = 0;
  virtual void ResultMultiplyAdd(uint32_t multiplier, uint32_t part) = 0;

  // Subclasses must call this to do all the work.
  void ParseInt();

  // Subclasses may override this.
  virtual void HandleSpecialCases() {}

  // Subclass constructors should call these for configuration before calling
  // ParseInt().
  void set_allow_binary_and_octal_prefixes() {
    allow_binary_and_octal_prefixes_ = true;
  }
  void set_disallow_trailing_junk() { allow_trailing_junk_ = false; }

  bool IsOneByte() const {
    return raw_one_byte_subject_ != nullptr ||
           String::IsOneByteRepresentationUnderneath(*subject_);
  }

  Vector<const uint8_t> GetOneByteVector() {
    if (raw_one_byte_subject_ != nullptr) {
      return Vector<const uint8_t>(raw_one_byte_subject_, length_);
    }
    DisallowHeapAllocation no_gc;
    return subject_->GetFlatContent(no_gc).ToOneByteVector();
  }

  Vector<const uc16> GetTwoByteVector() {
    DisallowHeapAllocation no_gc;
    return subject_->GetFlatContent(no_gc).ToUC16Vector();
  }

  // Subclasses get access to internal state:
  enum State { kRunning, kError, kJunk, kEmpty, kZero, kDone };

  enum class Sign { kNegative, kPositive, kNone };

  Isolate* isolate() { return isolate_; }
  int radix() { return radix_; }
  int cursor() { return cursor_; }
  int length() { return length_; }
  bool negative() { return sign_ == Sign::kNegative; }
  Sign sign() { return sign_; }
  State state() { return state_; }
  void set_state(State state) { state_ = state; }

 private:
  template <class Char>
  void DetectRadixInternal(Char current, int length);
  template <class Char>
  void ParseInternal(Char start);

  Isolate* isolate_;
  Handle<String> subject_;
  const uint8_t* raw_one_byte_subject_ = nullptr;
  int radix_ = 0;
  int cursor_ = 0;
  int length_ = 0;
  Sign sign_ = Sign::kNone;
  bool leading_zero_ = false;
  bool allow_binary_and_octal_prefixes_ = false;
  bool allow_trailing_junk_ = true;
  State state_ = kRunning;
};

void StringToIntHelper::ParseInt() {
  {
    DisallowHeapAllocation no_gc;
    if (IsOneByte()) {
      Vector<const uint8_t> vector = GetOneByteVector();
      DetectRadixInternal(vector.start(), vector.length());
    } else {
      Vector<const uc16> vector = GetTwoByteVector();
      DetectRadixInternal(vector.start(), vector.length());
    }
  }
  if (state_ != kRunning) return;
  AllocateResult();
  HandleSpecialCases();
  if (state_ != kRunning) return;
  {
    DisallowHeapAllocation no_gc;
    if (IsOneByte()) {
      Vector<const uint8_t> vector = GetOneByteVector();
      DCHECK_EQ(length_, vector.length());
      ParseInternal(vector.start());
    } else {
      Vector<const uc16> vector = GetTwoByteVector();
      DCHECK_EQ(length_, vector.length());
      ParseInternal(vector.start());
    }
  }
  DCHECK_NE(state_, kRunning);
}

template <class Char>
void StringToIntHelper::DetectRadixInternal(Char current, int length) {
  Char start = current;
  length_ = length;
  Char end = start + length;

  if (!AdvanceToNonspace(&current, end)) {
    return set_state(kEmpty);
  }

  if (*current == '+') {
    // Ignore leading sign; skip following spaces.
    ++current;
    if (current == end) {
      return set_state(kJunk);
    }
    sign_ = Sign::kPositive;
  } else if (*current == '-') {
    ++current;
    if (current == end) {
      return set_state(kJunk);
    }
    sign_ = Sign::kNegative;
  }

  if (radix_ == 0) {
    // Radix detection.
    radix_ = 10;
    if (*current == '0') {
      ++current;
      if (current == end) return set_state(kZero);
      if (*current == 'x' || *current == 'X') {
        radix_ = 16;
        ++current;
        if (current == end) return set_state(kJunk);
      } else if (allow_binary_and_octal_prefixes_ &&
                 (*current == 'o' || *current == 'O')) {
        radix_ = 8;
        ++current;
        if (current == end) return set_state(kJunk);
      } else if (allow_binary_and_octal_prefixes_ &&
                 (*current == 'b' || *current == 'B')) {
        radix_ = 2;
        ++current;
        if (current == end) return set_state(kJunk);
      } else {
        leading_zero_ = true;
      }
    }
  } else if (radix_ == 16) {
    if (*current == '0') {
      // Allow "0x" prefix.
      ++current;
      if (current == end) return set_state(kZero);
      if (*current == 'x' || *current == 'X') {
        ++current;
        if (current == end) return set_state(kJunk);
      } else {
        leading_zero_ = true;
      }
    }
  }
  // Skip leading zeros.
  while (*current == '0') {
    leading_zero_ = true;
    ++current;
    if (current == end) return set_state(kZero);
  }

  if (!leading_zero_ && !isDigit(*current, radix_)) {
    return set_state(kJunk);
  }

  DCHECK(radix_ >= 2 && radix_ <= 36);
  STATIC_ASSERT(String::kMaxLength <= INT_MAX);
  cursor_ = static_cast<int>(current - start);
}

template <class Char>
void StringToIntHelper::ParseInternal(Char start) {
  Char current = start + cursor_;
  Char end = start + length_;

  // The following code causes accumulating rounding error for numbers greater
  // than ~2^56. It's explicitly allowed in the spec: "if R is not 2, 4, 8, 10,
  // 16, or 32, then mathInt may be an implementation-dependent approximation to
  // the mathematical integer value" (15.1.2.2).

  int lim_0 = '0' + (radix_ < 10 ? radix_ : 10);
  int lim_a = 'a' + (radix_ - 10);
  int lim_A = 'A' + (radix_ - 10);

  // NOTE: The code for computing the value may seem a bit complex at
  // first glance. It is structured to use 32-bit multiply-and-add
  // loops as long as possible to avoid losing precision.

  bool done = false;
  do {
    // Parse the longest part of the string starting at {current}
    // possible while keeping the multiplier, and thus the part
    // itself, within 32 bits.
    uint32_t part = 0, multiplier = 1;
    while (true) {
      uint32_t d;
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
      const uint32_t kMaximumMultiplier = 0xFFFFFFFFU / 36;
      uint32_t m = multiplier * static_cast<uint32_t>(radix_);
      if (m > kMaximumMultiplier) break;
      part = part * radix_ + d;
      multiplier = m;
      DCHECK(multiplier > part);

      ++current;
      if (current == end) {
        done = true;
        break;
      }
    }

    // Update the value and skip the part in the string.
    ResultMultiplyAdd(multiplier, part);
  } while (!done);

  if (!allow_trailing_junk_ && AdvanceToNonspace(&current, end)) {
    return set_state(kJunk);
  }

  return set_state(kDone);
}

class NumberParseIntHelper : public StringToIntHelper {
 public:
  NumberParseIntHelper(Isolate* isolate, Handle<String> string, int radix)
      : StringToIntHelper(isolate, string, radix) {}

  double GetResult() {
    ParseInt();
    switch (state()) {
      case kJunk:
      case kEmpty:
        return JunkStringValue();
      case kZero:
        return SignedZero(negative());
      case kDone:
        return negative() ? -result_ : result_;
      case kError:
      case kRunning:
        break;
    }
    UNREACHABLE();
  }

 protected:
  void AllocateResult() override {}
  void ResultMultiplyAdd(uint32_t multiplier, uint32_t part) override {
    result_ = result_ * multiplier + part;
  }

 private:
  void HandleSpecialCases() override {
    bool is_power_of_two = base::bits::IsPowerOfTwo(radix());
    if (!is_power_of_two && radix() != 10) return;
    DisallowHeapAllocation no_gc;
    if (IsOneByte()) {
      Vector<const uint8_t> vector = GetOneByteVector();
      DCHECK_EQ(length(), vector.length());
      result_ = is_power_of_two ? HandlePowerOfTwoCase(vector.start())
                                : HandleBaseTenCase(vector.start());
    } else {
      Vector<const uc16> vector = GetTwoByteVector();
      DCHECK_EQ(length(), vector.length());
      result_ = is_power_of_two ? HandlePowerOfTwoCase(vector.start())
                                : HandleBaseTenCase(vector.start());
    }
    set_state(kDone);
  }

  template <class Char>
  double HandlePowerOfTwoCase(Char start) {
    Char current = start + cursor();
    Char end = start + length();
    const bool allow_trailing_junk = true;
    // GetResult() will take care of the sign bit, so ignore it for now.
    const bool negative = false;
    switch (radix()) {
      case 2:
        return InternalStringToIntDouble<1>(current, end, negative,
                                            allow_trailing_junk);
      case 4:
        return InternalStringToIntDouble<2>(current, end, negative,
                                            allow_trailing_junk);
      case 8:
        return InternalStringToIntDouble<3>(current, end, negative,
                                            allow_trailing_junk);

      case 16:
        return InternalStringToIntDouble<4>(current, end, negative,
                                            allow_trailing_junk);

      case 32:
        return InternalStringToIntDouble<5>(current, end, negative,
                                            allow_trailing_junk);
      default:
        UNREACHABLE();
    }
  }

  template <class Char>
  double HandleBaseTenCase(Char start) {
    // Parsing with strtod.
    Char current = start + cursor();
    Char end = start + length();
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
        DCHECK_LT(buffer_pos, kBufferSize);
        buffer[buffer_pos++] = static_cast<char>(*current);
      }
      ++current;
      if (current == end) break;
    }

    SLOW_DCHECK(buffer_pos < kBufferSize);
    buffer[buffer_pos] = '\0';
    Vector<const char> buffer_vector(buffer, buffer_pos);
    return Strtod(buffer_vector, 0);
  }

  double result_ = 0;
};

// Converts a string to a double value. Assumes the Iterator supports
// the following operations:
// 1. current == end (other ops are not allowed), current != end.
// 2. *current - gets the current character in the sequence.
// 3. ++current (advances the position).
template <class Iterator, class EndMark>
double InternalStringToDouble(Iterator current, EndMark end, int flags,
                              double empty_string_val) {
  // To make sure that iterator dereferencing is valid the following
  // convention is used:
  // 1. Each '++current' statement is followed by check for equality to 'end'.
  // 2. If AdvanceToNonspace returned false then current == end.
  // 3. If 'current' becomes be equal to 'end' the function returns or goes to
  // 'parsing_done'.
  // 4. 'current' is not dereferenced after the 'parsing_done' label.
  // 5. Code before 'parsing_done' may rely on 'current != end'.
  if (!AdvanceToNonspace(&current, end)) {
    return empty_string_val;
  }

  const bool allow_trailing_junk = (flags & ALLOW_TRAILING_JUNK) != 0;

  // Maximum number of significant digits in decimal representation.
  // The longest possible double in decimal representation is
  // (2^53 - 1) * 2 ^ -1074 that is (2 ^ 53 - 1) * 5 ^ 1074 / 10 ^ 1074
  // (768 digits). If we parse a number whose first digits are equal to a
  // mean of 2 adjacent doubles (that could have up to 769 digits) the result
  // must be rounded to the bigger one unless the tail consists of zeros, so
  // we don't need to preserve all the digits.
  const int kMaxSignificantDigits = 772;

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

  enum Sign { NONE, NEGATIVE, POSITIVE };

  Sign sign = NONE;

  if (*current == '+') {
    // Ignore leading sign.
    ++current;
    if (current == end) return JunkStringValue();
    sign = POSITIVE;
  } else if (*current == '-') {
    ++current;
    if (current == end) return JunkStringValue();
    sign = NEGATIVE;
  }

  static const char kInfinityString[] = "Infinity";
  if (*current == kInfinityString[0]) {
    if (!SubStringEquals(&current, end, kInfinityString)) {
      return JunkStringValue();
    }

    if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
      return JunkStringValue();
    }

    DCHECK_EQ(buffer_pos, 0);
    return (sign == NEGATIVE) ? -V8_INFINITY : V8_INFINITY;
  }

  bool leading_zero = false;
  if (*current == '0') {
    ++current;
    if (current == end) return SignedZero(sign == NEGATIVE);

    leading_zero = true;

    // It could be hexadecimal value.
    if ((flags & ALLOW_HEX) && (*current == 'x' || *current == 'X')) {
      ++current;
      if (current == end || !isDigit(*current, 16) || sign != NONE) {
        return JunkStringValue();  // "0x".
      }

      return InternalStringToIntDouble<4>(current, end, false,
                                          allow_trailing_junk);

      // It could be an explicit octal value.
    } else if ((flags & ALLOW_OCTAL) && (*current == 'o' || *current == 'O')) {
      ++current;
      if (current == end || !isDigit(*current, 8) || sign != NONE) {
        return JunkStringValue();  // "0o".
      }

      return InternalStringToIntDouble<3>(current, end, false,
                                          allow_trailing_junk);

      // It could be a binary value.
    } else if ((flags & ALLOW_BINARY) && (*current == 'b' || *current == 'B')) {
      ++current;
      if (current == end || !isBinaryDigit(*current) || sign != NONE) {
        return JunkStringValue();  // "0b".
      }

      return InternalStringToIntDouble<1>(current, end, false,
                                          allow_trailing_junk);
    }

    // Ignore leading zeros in the integer part.
    while (*current == '0') {
      ++current;
      if (current == end) return SignedZero(sign == NEGATIVE);
    }
  }

  bool octal = leading_zero && (flags & ALLOW_IMPLICIT_OCTAL) != 0;

  // Copy significant digits of the integer part (if any) to the buffer.
  while (*current >= '0' && *current <= '9') {
    if (significant_digits < kMaxSignificantDigits) {
      DCHECK_LT(buffer_pos, kBufferSize);
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
        if (current == end) return SignedZero(sign == NEGATIVE);
        exponent--;  // Move this 0 into the exponent.
      }
    }

    // There is a fractional part.  We don't emit a '.', but adjust the exponent
    // instead.
    while (*current >= '0' && *current <= '9') {
      if (significant_digits < kMaxSignificantDigits) {
        DCHECK_LT(buffer_pos, kBufferSize);
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
    DCHECK(-max_exponent / 2 <= exponent && exponent <= max_exponent / 2);
    int num = 0;
    do {
      // Check overflow.
      int digit = *current - '0';
      if (num >= max_exponent / 10 &&
          !(num == max_exponent / 10 && digit <= max_exponent % 10)) {
        num = max_exponent;
      } else {
        num = num * 10 + digit;
      }
      ++current;
    } while (current != end && *current >= '0' && *current <= '9');

    exponent += (sign == '-' ? -num : num);
  }

  if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
    return JunkStringValue();
  }

parsing_done:
  exponent += insignificant_digits;

  if (octal) {
    return InternalStringToIntDouble<3>(buffer, buffer + buffer_pos,
                                        sign == NEGATIVE, allow_trailing_junk);
  }

  if (nonzero_digit_dropped) {
    buffer[buffer_pos++] = '1';
    exponent--;
  }

  SLOW_DCHECK(buffer_pos < kBufferSize);
  buffer[buffer_pos] = '\0';

  double converted = Strtod(Vector<const char>(buffer, buffer_pos), exponent);
  return (sign == NEGATIVE) ? -converted : converted;
}

double StringToDouble(const char* str, int flags, double empty_string_val) {
  // We cast to const uint8_t* here to avoid instantiating the
  // InternalStringToDouble() template for const char* as well.
  const uint8_t* start = reinterpret_cast<const uint8_t*>(str);
  const uint8_t* end = start + StrLength(str);
  return InternalStringToDouble(start, end, flags, empty_string_val);
}

double StringToDouble(Vector<const uint8_t> str, int flags,
                      double empty_string_val) {
  // We cast to const uint8_t* here to avoid instantiating the
  // InternalStringToDouble() template for const char* as well.
  const uint8_t* start = reinterpret_cast<const uint8_t*>(str.start());
  const uint8_t* end = start + str.length();
  return InternalStringToDouble(start, end, flags, empty_string_val);
}

double StringToDouble(Vector<const uc16> str, int flags,
                      double empty_string_val) {
  const uc16* end = str.start() + str.length();
  return InternalStringToDouble(str.start(), end, flags, empty_string_val);
}

double StringToInt(Isolate* isolate, Handle<String> string, int radix) {
  NumberParseIntHelper helper(isolate, string, radix);
  return helper.GetResult();
}

class StringToBigIntHelper : public StringToIntHelper {
 public:
  enum class Behavior { kStringToBigInt, kLiteral };

  // Used for StringToBigInt operation (BigInt constructor and == operator).
  StringToBigIntHelper(Isolate* isolate, Handle<String> string)
      : StringToIntHelper(isolate, string),
        behavior_(Behavior::kStringToBigInt) {
    set_allow_binary_and_octal_prefixes();
    set_disallow_trailing_junk();
  }

  // Used for parsing BigInt literals, where the input is a buffer of
  // one-byte ASCII digits, along with an optional radix prefix.
  StringToBigIntHelper(Isolate* isolate, const uint8_t* string, int length)
      : StringToIntHelper(isolate, string, length),
        behavior_(Behavior::kLiteral) {
    set_allow_binary_and_octal_prefixes();
  }

  MaybeHandle<BigInt> GetResult() {
    ParseInt();
    if (behavior_ == Behavior::kStringToBigInt && sign() != Sign::kNone &&
        radix() != 10) {
      return MaybeHandle<BigInt>();
    }
    if (state() == kEmpty) {
      if (behavior_ == Behavior::kStringToBigInt) {
        set_state(kZero);
      } else {
        UNREACHABLE();
      }
    }
    switch (state()) {
      case kJunk:
        if (should_throw() == kThrowOnError) {
          THROW_NEW_ERROR(isolate(),
                          NewSyntaxError(MessageTemplate::kBigIntInvalidString),
                          BigInt);
        } else {
          DCHECK_EQ(should_throw(), kDontThrow);
          return MaybeHandle<BigInt>();
        }
      case kZero:
        return BigInt::Zero(isolate());
      case kError:
        DCHECK_EQ(should_throw() == kThrowOnError,
                  isolate()->has_pending_exception());
        return MaybeHandle<BigInt>();
      case kDone:
        return BigInt::Finalize(result_, negative());
      case kEmpty:
      case kRunning:
        break;
    }
    UNREACHABLE();
  }

 protected:
  void AllocateResult() override {
    // We have to allocate a BigInt that's big enough to fit the result.
    // Conseratively assume that all remaining digits are significant.
    // Optimization opportunity: Would it makes sense to scan for trailing
    // junk before allocating the result?
    int charcount = length() - cursor();
    // For literals, we pretenure the allocated BigInt, since it's about
    // to be stored in the interpreter's constants array.
    AllocationType allocation = behavior_ == Behavior::kLiteral
                                    ? AllocationType::kOld
                                    : AllocationType::kYoung;
    MaybeHandle<FreshlyAllocatedBigInt> maybe = BigInt::AllocateFor(
        isolate(), radix(), charcount, should_throw(), allocation);
    if (!maybe.ToHandle(&result_)) {
      set_state(kError);
    }
  }

  void ResultMultiplyAdd(uint32_t multiplier, uint32_t part) override {
    BigInt::InplaceMultiplyAdd(result_, static_cast<uintptr_t>(multiplier),
                               static_cast<uintptr_t>(part));
  }

 private:
  ShouldThrow should_throw() const { return kDontThrow; }

  Handle<FreshlyAllocatedBigInt> result_;
  Behavior behavior_;
};

MaybeHandle<BigInt> StringToBigInt(Isolate* isolate, Handle<String> string) {
  string = String::Flatten(isolate, string);
  StringToBigIntHelper helper(isolate, string);
  return helper.GetResult();
}

MaybeHandle<BigInt> BigIntLiteral(Isolate* isolate, const char* string) {
  StringToBigIntHelper helper(isolate, reinterpret_cast<const uint8_t*>(string),
                              static_cast<int>(strlen(string)));
  return helper.GetResult();
}

const char* DoubleToCString(double v, Vector<char> buffer) {
  switch (FPCLASSIFY_NAMESPACE::fpclassify(v)) {
    case FP_NAN: return "NaN";
    case FP_INFINITE: return (v < 0.0 ? "-Infinity" : "Infinity");
    case FP_ZERO: return "0";
    default: {
      if (IsInt32Double(v)) {
        // This will trigger if v is -0 and -0.0 is stringified to "0".
        // (see ES section 7.1.12.1 #sec-tostring-applied-to-the-number-type)
        return IntToCString(FastD2I(v), buffer);
      }
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
  bool negative = true;
  if (n >= 0) {
    n = -n;
    negative = false;
  }
  // Build the string backwards from the least significant digit.
  int i = buffer.length();
  buffer[--i] = '\0';
  do {
    // We ensured n <= 0, so the subtraction does the right addition.
    buffer[--i] = '0' - (n % 10);
    n /= 10;
  } while (n);
  if (negative) buffer[--i] = '-';
  return buffer.start() + i;
}


char* DoubleToFixedCString(double value, int f) {
  const int kMaxDigitsBeforePoint = 21;
  const double kFirstNonFixed = 1e21;
  DCHECK_GE(f, 0);
  DCHECK_LE(f, kMaxFractionDigits);

  bool negative = false;
  double abs_value = value;
  if (value < 0) {
    abs_value = -value;
    negative = true;
  }

  // If abs_value has more than kMaxDigitsBeforePoint digits before the point
  // use the non-fixed conversion routine.
  if (abs_value >= kFirstNonFixed) {
    char arr[kMaxFractionDigits];
    Vector<char> buffer(arr, arraysize(arr));
    return StrDup(DoubleToCString(value, buffer));
  }

  // Find a sufficiently precise decimal representation of n.
  int decimal_point;
  int sign;
  // Add space for the '\0' byte.
  const int kDecimalRepCapacity =
      kMaxDigitsBeforePoint + kMaxFractionDigits + 1;
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
  // f might be -1 to signal that f was undefined in JavaScript.
  DCHECK(f >= -1 && f <= kMaxFractionDigits);

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
  const int kV8DtoaBufferCapacity = kMaxFractionDigits + 1 + 1;
  // Make sure that the buffer is big enough, even if we fall back to the
  // shortest representation (which happens when f equals -1).
  DCHECK_LE(kBase10MaximalLength, kMaxFractionDigits + 1);
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
  DCHECK_GT(decimal_rep_length, 0);
  DCHECK(decimal_rep_length <= f + 1);

  int exponent = decimal_point - 1;
  char* result =
      CreateExponentialRepresentation(decimal_rep, exponent, negative, f+1);

  return result;
}


char* DoubleToPrecisionCString(double value, int p) {
  const int kMinimalDigits = 1;
  DCHECK(p >= kMinimalDigits && p <= kMaxFractionDigits);
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
  const int kV8DtoaBufferCapacity = kMaxFractionDigits + 1;
  char decimal_rep[kV8DtoaBufferCapacity];
  int decimal_rep_length;

  DoubleToAscii(value, DTOA_PRECISION, p,
                Vector<char>(decimal_rep, kV8DtoaBufferCapacity),
                &sign, &decimal_rep_length, &decimal_point);
  DCHECK(decimal_rep_length <= p);

  int exponent = decimal_point - 1;

  char* result = nullptr;

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
  DCHECK(std::isfinite(value));
  DCHECK_NE(0.0, value);
  // Character array used for conversion.
  static const char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  // Temporary buffer for the result. We start with the decimal point in the
  // middle and write to the left for the integer part and to the right for the
  // fractional part. 1024 characters for the exponent and 52 for the mantissa
  // either way, with additional space for sign, decimal point and string
  // termination should be sufficient.
  static const int kBufferSize = 2200;
  char buffer[kBufferSize];
  int integer_cursor = kBufferSize / 2;
  int fraction_cursor = integer_cursor;

  bool negative = value < 0;
  if (negative) value = -value;

  // Split the value into an integer part and a fractional part.
  double integer = std::floor(value);
  double fraction = value - integer;
  // We only compute fractional digits up to the input double's precision.
  double delta = 0.5 * (Double(value).NextDouble() - value);
  delta = std::max(Double(0.0).NextDouble(), delta);
  DCHECK_GT(delta, 0.0);
  if (fraction > delta) {
    // Insert decimal point.
    buffer[fraction_cursor++] = '.';
    do {
      // Shift up by one digit.
      fraction *= radix;
      delta *= radix;
      // Write digit.
      int digit = static_cast<int>(fraction);
      buffer[fraction_cursor++] = chars[digit];
      // Calculate remainder.
      fraction -= digit;
      // Round to even.
      if (fraction > 0.5 || (fraction == 0.5 && (digit & 1))) {
        if (fraction + delta > 1) {
          // We need to back trace already written digits in case of carry-over.
          while (true) {
            fraction_cursor--;
            if (fraction_cursor == kBufferSize / 2) {
              CHECK_EQ('.', buffer[fraction_cursor]);
              // Carry over to the integer part.
              integer += 1;
              break;
            }
            char c = buffer[fraction_cursor];
            // Reconstruct digit.
            int digit = c > '9' ? (c - 'a' + 10) : (c - '0');
            if (digit + 1 < radix) {
              buffer[fraction_cursor++] = chars[digit + 1];
              break;
            }
          }
          break;
        }
      }
    } while (fraction > delta);
  }

  // Compute integer digits. Fill unrepresented digits with zero.
  while (Double(integer / radix).Exponent() > 0) {
    integer /= radix;
    buffer[--integer_cursor] = '0';
  }
  do {
    double remainder = Modulo(integer, radix);
    buffer[--integer_cursor] = chars[static_cast<int>(remainder)];
    integer = (integer - remainder) / radix;
  } while (integer > 0);

  // Add sign and terminate string.
  if (negative) buffer[--integer_cursor] = '-';
  buffer[fraction_cursor++] = '\0';
  DCHECK_LT(fraction_cursor, kBufferSize);
  DCHECK_LE(0, integer_cursor);
  // Allocate new string as return value.
  char* result = NewArray<char>(fraction_cursor - integer_cursor);
  memcpy(result, buffer + integer_cursor, fraction_cursor - integer_cursor);
  return result;
}


// ES6 18.2.4 parseFloat(string)
double StringToDouble(Isolate* isolate, Handle<String> string, int flags,
                      double empty_string_val) {
  Handle<String> flattened = String::Flatten(isolate, string);
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = flattened->GetFlatContent(no_gc);
    DCHECK(flat.IsFlat());
    if (flat.IsOneByte()) {
      return StringToDouble(flat.ToOneByteVector(), flags, empty_string_val);
    } else {
      return StringToDouble(flat.ToUC16Vector(), flags, empty_string_val);
    }
  }
}

bool IsSpecialIndex(String string) {
  // Max length of canonical double: -X.XXXXXXXXXXXXXXXXX-eXXX
  const int kBufferSize = 24;
  const int length = string->length();
  if (length == 0 || length > kBufferSize) return false;
  uint16_t buffer[kBufferSize];
  String::WriteToFlat(string, buffer, 0, length);
  // If the first char is not a digit or a '-' or we can't match 'NaN' or
  // '(-)Infinity', bailout immediately.
  int offset = 0;
  if (!IsDecimalDigit(buffer[0])) {
    if (buffer[0] == '-') {
      if (length == 1) return false;  // Just '-' is bad.
      if (!IsDecimalDigit(buffer[1])) {
        if (buffer[1] == 'I' && length == 9) {
          // Allow matching of '-Infinity' below.
        } else {
          return false;
        }
      }
      offset++;
    } else if (buffer[0] == 'I' && length == 8) {
      // Allow matching of 'Infinity' below.
    } else if (buffer[0] == 'N' && length == 3) {
      // Match NaN.
      return buffer[1] == 'a' && buffer[2] == 'N';
    } else {
      return false;
    }
  }
  // Expected fast path: key is an integer.
  static const int kRepresentableIntegerLength = 15;  // (-)XXXXXXXXXXXXXXX
  if (length - offset <= kRepresentableIntegerLength) {
    const int initial_offset = offset;
    bool matches = true;
    for (; offset < length; offset++) {
      matches &= IsDecimalDigit(buffer[offset]);
    }
    if (matches) {
      // Match 0 and -0.
      if (buffer[initial_offset] == '0') return initial_offset == length - 1;
      return true;
    }
  }
  // Slow path: test DoubleToString(StringToDouble(string)) == string.
  Vector<const uint16_t> vector(buffer, length);
  double d = StringToDouble(vector, NO_FLAGS);
  if (std::isnan(d)) return false;
  // Compute reverse string.
  char reverse_buffer[kBufferSize + 1];  // Result will be /0 terminated.
  Vector<char> reverse_vector(reverse_buffer, arraysize(reverse_buffer));
  const char* reverse_string = DoubleToCString(d, reverse_vector);
  for (int i = 0; i < length; ++i) {
    if (static_cast<uint16_t>(reverse_string[i]) != buffer[i]) return false;
  }
  return true;
}
}  // namespace internal
}  // namespace v8
