// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions that operate on individual digits.

#ifndef V8_BIGINT_DIGIT_ARITHMETIC_H_
#define V8_BIGINT_DIGIT_ARITHMETIC_H_

#include "src/bigint/bigint.h"
#include "src/bigint/util.h"

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

// {borrow} will be set to 0 or 1.
inline digit_t digit_sub(digit_t a, digit_t b, digit_t* borrow) {
#if HAVE_TWODIGIT_T
  twodigit_t result = twodigit_t{a} - b;
  *borrow = (result >> kDigitBits) & 1;
  return static_cast<digit_t>(result);
#else
  digit_t result = a - b;
  *borrow = (result > a) ? 1 : 0;
  return result;
#endif
}

// {borrow_out} will be set to 0 or 1.
inline digit_t digit_sub2(digit_t a, digit_t b, digit_t borrow_in,
                          digit_t* borrow_out) {
#if HAVE_TWODIGIT_T
  twodigit_t subtrahend = twodigit_t{b} + borrow_in;
  twodigit_t result = twodigit_t{a} - subtrahend;
  *borrow_out = (result >> kDigitBits) & 1;
  return static_cast<digit_t>(result);
#else
  digit_t result = a - b;
  *borrow_out = (result > a) ? 1 : 0;
  if (result < borrow_in) *borrow_out += 1;
  result -= borrow_in;
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

// Returns the quotient.
// quotient = (high << kDigitBits + low - remainder) / divisor
static inline digit_t digit_div(digit_t high, digit_t low, digit_t divisor,
                                digit_t* remainder) {
#if defined(DCHECK)
  DCHECK(high < divisor);
  DCHECK(divisor != 0);  // NOLINT(readability/check)
#endif
#if __x86_64__ && (__GNUC__ || __clang__)
  digit_t quotient;
  digit_t rem;
  __asm__("divq  %[divisor]"
          // Outputs: {quotient} will be in rax, {rem} in rdx.
          : "=a"(quotient), "=d"(rem)
          // Inputs: put {high} into rdx, {low} into rax, and {divisor} into
          // any register or stack slot.
          : "d"(high), "a"(low), [divisor] "rm"(divisor));
  *remainder = rem;
  return quotient;
#elif __i386__ && (__GNUC__ || __clang__)
  digit_t quotient;
  digit_t rem;
  __asm__("divl  %[divisor]"
          // Outputs: {quotient} will be in eax, {rem} in edx.
          : "=a"(quotient), "=d"(rem)
          // Inputs: put {high} into edx, {low} into eax, and {divisor} into
          // any register or stack slot.
          : "d"(high), "a"(low), [divisor] "rm"(divisor));
  *remainder = rem;
  return quotient;
#else
  // Adapted from Warren, Hacker's Delight, p. 152.
  int s = CountLeadingZeros(divisor);
#if defined(DCHECK)
  DCHECK(s != kDigitBits);  // {divisor} is not 0.
#endif
  divisor <<= s;

  digit_t vn1 = divisor >> kHalfDigitBits;
  digit_t vn0 = divisor & kHalfDigitMask;
  // {s} can be 0. {low >> kDigitBits} would be undefined behavior, so
  // we mask the shift amount with {kShiftMask}, and the result with
  // {s_zero_mask} which is 0 if s == 0 and all 1-bits otherwise.
  static_assert(sizeof(intptr_t) == sizeof(digit_t),
                "intptr_t and digit_t must have the same size");
  const int kShiftMask = kDigitBits - 1;
  digit_t s_zero_mask =
      static_cast<digit_t>(static_cast<intptr_t>(-s) >> (kDigitBits - 1));
  digit_t un32 =
      (high << s) | ((low >> ((kDigitBits - s) & kShiftMask)) & s_zero_mask);
  digit_t un10 = low << s;
  digit_t un1 = un10 >> kHalfDigitBits;
  digit_t un0 = un10 & kHalfDigitMask;
  digit_t q1 = un32 / vn1;
  digit_t rhat = un32 - q1 * vn1;

  while (q1 >= kHalfDigitBase || q1 * vn0 > rhat * kHalfDigitBase + un1) {
    q1--;
    rhat += vn1;
    if (rhat >= kHalfDigitBase) break;
  }

  digit_t un21 = un32 * kHalfDigitBase + un1 - q1 * divisor;
  digit_t q0 = un21 / vn1;
  rhat = un21 - q0 * vn1;

  while (q0 >= kHalfDigitBase || q0 * vn0 > rhat * kHalfDigitBase + un0) {
    q0--;
    rhat += vn1;
    if (rhat >= kHalfDigitBase) break;
  }

  *remainder = (un21 * kHalfDigitBase + un0 - q0 * divisor) >> s;
  return q1 * kHalfDigitBase + q0;
#endif
}

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_DIGIT_ARITHMETIC_H_
