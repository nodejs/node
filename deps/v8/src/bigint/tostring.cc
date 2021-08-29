// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <limits>

#include "src/bigint/bigint-internal.h"
#include "src/bigint/digit-arithmetic.h"
#include "src/bigint/div-helpers.h"
#include "src/bigint/util.h"
#include "src/bigint/vector-arithmetic.h"

namespace v8 {
namespace bigint {

namespace {

// Lookup table for the maximum number of bits required per character of a
// base-N string representation of a number. To increase accuracy, the array
// value is the actual value multiplied by 32. To generate this table:
// for (var i = 0; i <= 36; i++) { print(Math.ceil(Math.log2(i) * 32) + ","); }
constexpr uint8_t kMaxBitsPerChar[] = {
    0,   0,   32,  51,  64,  75,  83,  90,  96,  // 0..8
    102, 107, 111, 115, 119, 122, 126, 128,      // 9..16
    131, 134, 136, 139, 141, 143, 145, 147,      // 17..24
    149, 151, 153, 154, 156, 158, 159, 160,      // 25..32
    162, 163, 165, 166,                          // 33..36
};

static const int kBitsPerCharTableShift = 5;
static const size_t kBitsPerCharTableMultiplier = 1u << kBitsPerCharTableShift;

static const char kConversionChars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

// Raises {base} to the power of {exponent}. Does not check for overflow.
digit_t digit_pow(digit_t base, digit_t exponent) {
  digit_t result = 1ull;
  while (exponent > 0) {
    if (exponent & 1) {
      result *= base;
    }
    exponent >>= 1;
    base *= base;
  }
  return result;
}

// Compile-time version of the above.
constexpr digit_t digit_pow_rec(digit_t base, digit_t exponent) {
  return exponent == 1 ? base : base * digit_pow_rec(base, exponent - 1);
}

// A variant of ToStringFormatter::BasecaseLast, specialized for a radix
// known at compile-time.
template <int radix>
char* BasecaseFixedLast(digit_t chunk, char* out) {
  while (chunk != 0) {
    DCHECK(*(out - 1) == kStringZapValue);  // NOLINT(readability/check)
    if (radix <= 10) {
      *(--out) = '0' + (chunk % radix);
    } else {
      *(--out) = kConversionChars[chunk % radix];
    }
    chunk /= radix;
  }
  return out;
}

// By making {radix} a compile-time constant and computing {chunk_divisor}
// as another compile-time constant from it, we allow the compiler to emit
// an optimized instruction sequence based on multiplications with "magic"
// numbers (modular multiplicative inverses) instead of actual divisions.
// The price we pay is having to work on half digits; the technique doesn't
// work with twodigit_t-by-digit_t divisions.
// Includes an equivalent of ToStringFormatter::BasecaseMiddle, accordingly
// specialized for a radix known at compile time.
template <digit_t radix>
char* DivideByMagic(RWDigits rest, Digits input, char* output) {
  constexpr uint8_t max_bits_per_char = kMaxBitsPerChar[radix];
  constexpr int chunk_chars =
      kHalfDigitBits * kBitsPerCharTableMultiplier / max_bits_per_char;
  constexpr digit_t chunk_divisor = digit_pow_rec(radix, chunk_chars);
  digit_t remainder = 0;
  for (int i = input.len() - 1; i >= 0; i--) {
    digit_t d = input[i];
    digit_t upper = (remainder << kHalfDigitBits) | (d >> kHalfDigitBits);
    digit_t u_result = upper / chunk_divisor;
    remainder = upper % chunk_divisor;
    digit_t lower = (remainder << kHalfDigitBits) | (d & kHalfDigitMask);
    digit_t l_result = lower / chunk_divisor;
    remainder = lower % chunk_divisor;
    rest[i] = (u_result << kHalfDigitBits) | l_result;
  }
  // {remainder} is now the current chunk to be written out.
  for (int i = 0; i < chunk_chars; i++) {
    DCHECK(*(output - 1) == kStringZapValue);  // NOLINT(readability/check)
    if (radix <= 10) {
      *(--output) = '0' + (remainder % radix);
    } else {
      *(--output) = kConversionChars[remainder % radix];
    }
    remainder /= radix;
  }
  DCHECK(remainder == 0);  // NOLINT(readability/check)
  return output;
}

class ToStringFormatter {
 public:
  ToStringFormatter(Digits X, int radix, bool sign, char* out,
                    int chars_available, ProcessorImpl* processor)
      : digits_(X),
        radix_(radix),
        sign_(sign),
        out_start_(out),
        out_end_(out + chars_available),
        out_(out_end_),
        processor_(processor) {
    DCHECK(chars_available >= ToStringResultLength(digits_, radix_, sign_));
  }

  void Start();
  int Finish();

  void Classic() {
    if (digits_.len() == 0) {
      *(--out_) = '0';
      return;
    }
    if (digits_.len() == 1) {
      out_ = BasecaseLast(digits_[0], out_);
      return;
    }
    // {rest} holds the part of the BigInt that we haven't looked at yet.
    // Not to be confused with "remainder"!
    ScratchDigits rest(digits_.len());
    // In the first round, divide the input, allocating a new BigInt for
    // the result == rest; from then on divide the rest in-place.
    Digits dividend = digits_;
    do {
      if (radix_ == 10) {
        // Faster but costs binary size, so we optimize the most common case.
        out_ = DivideByMagic<10>(rest, dividend, out_);
        processor_->AddWorkEstimate(rest.len() * 2);
      } else {
        digit_t chunk;
        processor_->DivideSingle(rest, &chunk, dividend, chunk_divisor_);
        out_ = BasecaseMiddle(chunk, out_);
        // Assume that a division is about ten times as expensive as a
        // multiplication.
        processor_->AddWorkEstimate(rest.len() * 10);
      }
      if (processor_->should_terminate()) return;
      rest.Normalize();
      dividend = rest;
    } while (rest.len() > 1);
    out_ = BasecaseLast(rest[0], out_);
  }

  void BasePowerOfTwo();

 private:
  // When processing the last (most significant) digit, don't write leading
  // zeros.
  char* BasecaseLast(digit_t digit, char* out) {
    if (radix_ == 10) return BasecaseFixedLast<10>(digit, out);
    do {
      DCHECK(*(out - 1) == kStringZapValue);  // NOLINT(readability/check)
      *(--out) = kConversionChars[digit % radix_];
      digit /= radix_;
    } while (digit > 0);
    return out;
  }

  // When processing a middle (non-most significant) digit, always write the
  // same number of characters (as many '0' as necessary).
  char* BasecaseMiddle(digit_t digit, char* out) {
    for (int i = 0; i < chunk_chars_; i++) {
      DCHECK(*(out - 1) == kStringZapValue);  // NOLINT(readability/check)
      *(--out) = kConversionChars[digit % radix_];
      digit /= radix_;
    }
    DCHECK(digit == 0);  // NOLINT(readability/check)
    return out;
  }

  Digits digits_;
  int radix_;
  int max_bits_per_char_ = 0;
  int chunk_chars_ = 0;
  bool sign_;
  char* out_start_;
  char* out_end_;
  char* out_;
  digit_t chunk_divisor_ = 0;
  ProcessorImpl* processor_;
};

// Prepares data for {Classic}. Not needed for {BasePowerOfTwo}.
void ToStringFormatter::Start() {
  max_bits_per_char_ = kMaxBitsPerChar[radix_];
  chunk_chars_ = kDigitBits * kBitsPerCharTableMultiplier / max_bits_per_char_;
  chunk_divisor_ = digit_pow(radix_, chunk_chars_);
  // By construction of chunk_chars_, there can't have been overflow.
  DCHECK(chunk_divisor_ != 0);  // NOLINT(readability/check)
}

int ToStringFormatter::Finish() {
  DCHECK(out_ >= out_start_);
  DCHECK(out_ < out_end_);  // At least one character was written.
  while (out_ < out_end_ && *out_ == '0') out_++;
  if (sign_) *(--out_) = '-';
  int excess = 0;
  if (out_ > out_start_) {
    size_t actual_length = out_end_ - out_;
    excess = static_cast<int>(out_ - out_start_);
    std::memmove(out_start_, out_, actual_length);
  }
  return excess;
}

void ToStringFormatter::BasePowerOfTwo() {
  const int bits_per_char = CountTrailingZeros(radix_);
  const int char_mask = radix_ - 1;
  digit_t digit = 0;
  // Keeps track of how many unprocessed bits there are in {digit}.
  int available_bits = 0;
  for (int i = 0; i < digits_.len() - 1; i++) {
    digit_t new_digit = digits_[i];
    // Take any leftover bits from the last iteration into account.
    int current = (digit | (new_digit << available_bits)) & char_mask;
    *(--out_) = kConversionChars[current];
    int consumed_bits = bits_per_char - available_bits;
    digit = new_digit >> consumed_bits;
    available_bits = kDigitBits - consumed_bits;
    while (available_bits >= bits_per_char) {
      *(--out_) = kConversionChars[digit & char_mask];
      digit >>= bits_per_char;
      available_bits -= bits_per_char;
    }
  }
  // Take any leftover bits from the last iteration into account.
  digit_t msd = digits_.msd();
  int current = (digit | (msd << available_bits)) & char_mask;
  *(--out_) = kConversionChars[current];
  digit = msd >> (bits_per_char - available_bits);
  while (digit != 0) {
    *(--out_) = kConversionChars[digit & char_mask];
    digit >>= bits_per_char;
  }
}

}  // namespace

void ProcessorImpl::ToString(char* out, int* out_length, Digits X, int radix,
                             bool sign) {
#if DEBUG
  for (int i = 0; i < *out_length; i++) out[i] = kStringZapValue;
#endif
  ToStringFormatter formatter(X, radix, sign, out, *out_length, this);
  if (IsPowerOfTwo(radix)) {
    formatter.BasePowerOfTwo();
  } else {
    formatter.Start();
    formatter.Classic();
  }
  int excess = formatter.Finish();
  *out_length -= excess;
}

Status Processor::ToString(char* out, int* out_length, Digits X, int radix,
                           bool sign) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  impl->ToString(out, out_length, X, radix, sign);
  return impl->get_and_clear_status();
}

int ToStringResultLength(Digits X, int radix, bool sign) {
  const int bit_length = BitLength(X);
  int result;
  if (IsPowerOfTwo(radix)) {
    const int bits_per_char = CountTrailingZeros(radix);
    result = DIV_CEIL(bit_length, bits_per_char) + sign;
  } else {
    // Maximum number of bits we can represent with one character.
    const uint8_t max_bits_per_char = kMaxBitsPerChar[radix];
    // For estimating the result length, we have to be pessimistic and work with
    // the minimum number of bits one character can represent.
    const uint8_t min_bits_per_char = max_bits_per_char - 1;
    // Perform the following computation with uint64_t to avoid overflows.
    uint64_t chars_required = bit_length;
    chars_required *= kBitsPerCharTableMultiplier;
    chars_required = DIV_CEIL(chars_required, min_bits_per_char);
    DCHECK(chars_required < std::numeric_limits<int>::max());
    result = static_cast<int>(chars_required);
  }
  result += sign;
  return result;
}

}  // namespace bigint
}  // namespace v8
