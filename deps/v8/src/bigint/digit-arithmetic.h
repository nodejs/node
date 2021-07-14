// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions that operate on individual digits.

#ifndef V8_BIGINT_DIGIT_ARITHMETIC_H_
#define V8_BIGINT_DIGIT_ARITHMETIC_H_

#include "src/bigint/bigint.h"

namespace v8 {
namespace bigint {

static constexpr int kHalfDigitBits = kDigitBits / 2;
static constexpr digit_t kHalfDigitBase = digit_t{1} << kHalfDigitBits;
static constexpr digit_t kHalfDigitMask = kHalfDigitBase - 1;

// {carry} will be set to 0 or 1.
inline digit_t digit_add2(digit_t a, digit_t b, digit_t* carry) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} + b;
  *carry = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  digit_t result = a + b;
  *carry = (result < a) ? 1 : 0;
  return result;
#endif
}

// This compiles to slightly better machine code than repeated invocations
// of {digit_add2}.
inline digit_t digit_add3(digit_t a, digit_t b, digit_t c, digit_t* carry) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} + b + c;
  *carry = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  digit_t result = a + b;
  *carry = (result < a) ? 1 : 0;
  result += c;
  if (result < c) *carry += 1;
  return result;
#endif
}

// Returns the low half of the result. High half is in {high}.
inline digit_t digit_mul(digit_t a, digit_t b, digit_t* high) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} * b;
  *high = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  // Multiply in half-pointer-sized chunks.
  // For inputs [AH AL]*[BH BL], the result is:
  //
  //            [AL*BL]  // r_low
  //    +    [AL*BH]     // r_mid1
  //    +    [AH*BL]     // r_mid2
  //    + [AH*BH]        // r_high
  //    = [R4 R3 R2 R1]  // high = [R4 R3], low = [R2 R1]
  //
  // Where of course we must be careful with carries between the columns.
  digit_t a_low = a & kHalfDigitMask;
  digit_t a_high = a >> kHalfDigitBits;
  digit_t b_low = b & kHalfDigitMask;
  digit_t b_high = b >> kHalfDigitBits;

  digit_t r_low = a_low * b_low;
  digit_t r_mid1 = a_low * b_high;
  digit_t r_mid2 = a_high * b_low;
  digit_t r_high = a_high * b_high;

  digit_t carry = 0;
  digit_t low = digit_add3(r_low, r_mid1 << kHalfDigitBits,
                           r_mid2 << kHalfDigitBits, &carry);
  *high =
      (r_mid1 >> kHalfDigitBits) + (r_mid2 >> kHalfDigitBits) + r_high + carry;
  return low;
#endif
}

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_DIGIT_ARITHMETIC_H_
