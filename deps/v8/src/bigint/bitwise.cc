// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bigint/bigint-internal.h"
#include "src/bigint/digit-arithmetic.h"
#include "src/bigint/util.h"
#include "src/bigint/vector-arithmetic.h"

namespace v8 {
namespace bigint {

void BitwiseAnd_PosPos(RWDigits Z, Digits X, Digits Y) {
  int pairs = std::min(X.len(), Y.len());
  DCHECK(Z.len() >= pairs);
  int i = 0;
  for (; i < pairs; i++) Z[i] = X[i] & Y[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

void BitwiseAnd_NegNeg(RWDigits Z, Digits X, Digits Y) {
  // (-x) & (-y) == ~(x-1) & ~(y-1)
  //             == ~((x-1) | (y-1))
  //             == -(((x-1) | (y-1)) + 1)
  int pairs = std::min(X.len(), Y.len());
  digit_t x_borrow = 1;
  digit_t y_borrow = 1;
  int i = 0;
  for (; i < pairs; i++) {
    Z[i] = digit_sub(X[i], x_borrow, &x_borrow) |
           digit_sub(Y[i], y_borrow, &y_borrow);
  }
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = digit_sub(X[i], x_borrow, &x_borrow);
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], y_borrow, &y_borrow);
  DCHECK(x_borrow == 0);
  DCHECK(y_borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

void BitwiseAnd_PosNeg(RWDigits Z, Digits X, Digits Y) {
  // x & (-y) == x & ~(y-1)
  int pairs = std::min(X.len(), Y.len());
  digit_t borrow = 1;
  int i = 0;
  for (; i < pairs; i++) Z[i] = X[i] & ~digit_sub(Y[i], borrow, &borrow);
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

void BitwiseOr_PosPos(RWDigits Z, Digits X, Digits Y) {
  int pairs = std::min(X.len(), Y.len());
  int i = 0;
  for (; i < pairs; i++) Z[i] = X[i] | Y[i];
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Y.len(); i++) Z[i] = Y[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

void BitwiseOr_NegNeg(RWDigits Z, Digits X, Digits Y) {
  // (-x) | (-y) == ~(x-1) | ~(y-1)
  //             == ~((x-1) & (y-1))
  //             == -(((x-1) & (y-1)) + 1)
  int pairs = std::min(X.len(), Y.len());
  digit_t x_borrow = 1;
  digit_t y_borrow = 1;
  int i = 0;
  for (; i < pairs; i++) {
    Z[i] = digit_sub(X[i], x_borrow, &x_borrow) &
           digit_sub(Y[i], y_borrow, &y_borrow);
  }
  // Any leftover borrows don't matter, the '&' would drop them anyway.
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

void BitwiseOr_PosNeg(RWDigits Z, Digits X, Digits Y) {
  // x | (-y) == x | ~(y-1) == ~((y-1) &~ x) == -(((y-1) &~ x) + 1)
  int pairs = std::min(X.len(), Y.len());
  digit_t borrow = 1;
  int i = 0;
  for (; i < pairs; i++) Z[i] = digit_sub(Y[i], borrow, &borrow) & ~X[i];
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], borrow, &borrow);
  DCHECK(borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

void BitwiseXor_PosPos(RWDigits Z, Digits X, Digits Y) {
  int pairs = X.len();
  if (Y.len() < X.len()) {
    std::swap(X, Y);
    pairs = X.len();
  }
  DCHECK(X.len() <= Y.len());
  int i = 0;
  for (; i < pairs; i++) Z[i] = X[i] ^ Y[i];
  for (; i < Y.len(); i++) Z[i] = Y[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

void BitwiseXor_NegNeg(RWDigits Z, Digits X, Digits Y) {
  // (-x) ^ (-y) == ~(x-1) ^ ~(y-1) == (x-1) ^ (y-1)
  int pairs = std::min(X.len(), Y.len());
  digit_t x_borrow = 1;
  digit_t y_borrow = 1;
  int i = 0;
  for (; i < pairs; i++) {
    Z[i] = digit_sub(X[i], x_borrow, &x_borrow) ^
           digit_sub(Y[i], y_borrow, &y_borrow);
  }
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = digit_sub(X[i], x_borrow, &x_borrow);
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], y_borrow, &y_borrow);
  DCHECK(x_borrow == 0);
  DCHECK(y_borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
}

void BitwiseXor_PosNeg(RWDigits Z, Digits X, Digits Y) {
  // x ^ (-y) == x ^ ~(y-1) == ~(x ^ (y-1)) == -((x ^ (y-1)) + 1)
  int pairs = std::min(X.len(), Y.len());
  digit_t borrow = 1;
  int i = 0;
  for (; i < pairs; i++) Z[i] = X[i] ^ digit_sub(Y[i], borrow, &borrow);
  // (At least) one of the next two loops will perform zero iterations:
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Y.len(); i++) Z[i] = digit_sub(Y[i], borrow, &borrow);
  DCHECK(borrow == 0);
  for (; i < Z.len(); i++) Z[i] = 0;
  Add(Z, 1);
}

void LeftShift(RWDigits Z, Digits X, digit_t shift) {
  int digit_shift = static_cast<int>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);

  int i = 0;
  for (; i < digit_shift; ++i) Z[i] = 0;
  if (bits_shift == 0) {
    for (; i < X.len() + digit_shift; ++i) Z[i] = X[i - digit_shift];
    for (; i < Z.len(); ++i) Z[i] = 0;
  } else {
    digit_t carry = 0;
    for (; i < X.len() + digit_shift; ++i) {
      digit_t d = X[i - digit_shift];
      Z[i] = (d << bits_shift) | carry;
      carry = d >> (kDigitBits - bits_shift);
    }
    if (carry != 0) Z[i++] = carry;
    for (; i < Z.len(); ++i) Z[i] = 0;
  }
}

int RightShift_ResultLength(Digits X, bool x_sign, digit_t shift,
                            RightShiftState* state) {
  int digit_shift = static_cast<int>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);
  int result_length = X.len() - digit_shift;
  if (result_length <= 0) return 0;

  // For negative numbers, round down if any bit was shifted out (so that e.g.
  // -5n >> 1n == -3n and not -2n). Check now whether this will happen and
  // whether it can cause overflow into a new digit.
  bool must_round_down = false;
  if (x_sign) {
    const digit_t mask = (static_cast<digit_t>(1) << bits_shift) - 1;
    if ((X[digit_shift] & mask) != 0) {
      must_round_down = true;
    } else {
      for (int i = 0; i < digit_shift; i++) {
        if (X[i] != 0) {
          must_round_down = true;
          break;
        }
      }
    }
  }
  // If bits_shift is non-zero, it frees up bits, preventing overflow.
  if (must_round_down && bits_shift == 0) {
    // Overflow cannot happen if the most significant digit has unset bits.
    const bool rounding_can_overflow = digit_ismax(X.msd());
    if (rounding_can_overflow) ++result_length;
  }

  if (state) {
    DCHECK(!must_round_down || x_sign);
    state->must_round_down = must_round_down;
  }
  return result_length;
}

void RightShift(RWDigits Z, Digits X, digit_t shift,
                const RightShiftState& state) {
  int digit_shift = static_cast<int>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);

  int i = 0;
  if (bits_shift == 0) {
    for (; i < X.len() - digit_shift; ++i) Z[i] = X[i + digit_shift];
  } else {
    digit_t carry = X[digit_shift] >> bits_shift;
    for (; i < X.len() - digit_shift - 1; ++i) {
      digit_t d = X[i + digit_shift + 1];
      Z[i] = (d << (kDigitBits - bits_shift)) | carry;
      carry = d >> bits_shift;
    }
    Z[i++] = carry;
  }
  for (; i < Z.len(); ++i) Z[i] = 0;

  if (state.must_round_down) {
    // Rounding down (a negative value) means adding one to
    // its absolute value. This cannot overflow.
    Add(Z, 1);
  }
}

namespace {

// Z := (least significant n bits of X).
void TruncateToNBits(RWDigits Z, Digits X, int n) {
  int digits = DIV_CEIL(n, kDigitBits);
  int bits = n % kDigitBits;
  // Copy all digits except the MSD.
  int last = digits - 1;
  for (int i = 0; i < last; i++) {
    Z[i] = X[i];
  }
  // The MSD might contain extra bits that we don't want.
  digit_t msd = X[last];
  if (bits != 0) {
    int drop = kDigitBits - bits;
    msd = (msd << drop) >> drop;
  }
  Z[last] = msd;
}

// Z := 2**n - (least significant n bits of X).
void TruncateAndSubFromPowerOfTwo(RWDigits Z, Digits X, int n) {
  int digits = DIV_CEIL(n, kDigitBits);
  int bits = n % kDigitBits;
  // Process all digits except the MSD. Take X's digits, then simulate leading
  // zeroes.
  int last = digits - 1;
  int have_x = std::min(last, X.len());
  digit_t borrow = 0;
  int i = 0;
  for (; i < have_x; i++) Z[i] = digit_sub2(0, X[i], borrow, &borrow);
  for (; i < last; i++) Z[i] = digit_sub(0, borrow, &borrow);

  // The MSD might contain extra bits that we don't want.
  digit_t msd = last < X.len() ? X[last] : 0;
  if (bits == 0) {
    Z[last] = digit_sub2(0, msd, borrow, &borrow);
  } else {
    int drop = kDigitBits - bits;
    msd = (msd << drop) >> drop;
    digit_t minuend_msd = static_cast<digit_t>(1) << bits;
    digit_t result_msd = digit_sub2(minuend_msd, msd, borrow, &borrow);
    DCHECK(borrow == 0);  // result < 2^n.
    // If all subtracted bits were zero, we have to get rid of the
    // materialized minuend_msd again.
    Z[last] = result_msd & (minuend_msd - 1);
  }
}

}  // namespace

// Returns -1 when the operation would return X unchanged.
int AsIntNResultLength(Digits X, bool x_negative, int n) {
  int needed_digits = DIV_CEIL(n, kDigitBits);
  // Generally: decide based on number of digits, and bits in the top digit.
  if (X.len() < needed_digits) return -1;
  if (X.len() > needed_digits) return needed_digits;
  digit_t top_digit = X[needed_digits - 1];
  digit_t compare_digit = digit_t{1} << ((n - 1) % kDigitBits);
  if (top_digit < compare_digit) return -1;
  if (top_digit > compare_digit) return needed_digits;
  // Special case: if X == -2**(n-1), truncation is a no-op.
  if (!x_negative) return needed_digits;
  for (int i = needed_digits - 2; i >= 0; i--) {
    if (X[i] != 0) return needed_digits;
  }
  return -1;
}

bool AsIntN(RWDigits Z, Digits X, bool x_negative, int n) {
  DCHECK(X.len() > 0);
  DCHECK(n > 0);
  DCHECK(AsIntNResultLength(X, x_negative, n) > 0);
  int needed_digits = DIV_CEIL(n, kDigitBits);
  digit_t top_digit = X[needed_digits - 1];
  digit_t compare_digit = digit_t{1} << ((n - 1) % kDigitBits);
  // The canonical algorithm would be: convert negative numbers to two's
  // complement representation, truncate, convert back to sign+magnitude. To
  // avoid the conversions, we predict what the result would be:
  // When the (n-1)th bit is not set:
  //  - truncate the absolute value
  //  - preserve the sign.
  // When the (n-1)th bit is set:
  //  - subtract the truncated absolute value from 2**n to simulate two's
  //    complement representation
  //  - flip the sign, unless it's the special case where the input is negative
  //    and the result is the minimum n-bit integer. E.g. asIntN(3, -12) => -4.
  bool has_bit = (top_digit & compare_digit) == compare_digit;
  if (!has_bit) {
    TruncateToNBits(Z, X, n);
    return x_negative;
  }
  TruncateAndSubFromPowerOfTwo(Z, X, n);
  if (!x_negative) return true;  // Result is negative.
  // Scan for the special case (see above): if all bits below the (n-1)th
  // digit are zero, the result is negative.
  if ((top_digit & (compare_digit - 1)) != 0) return false;
  for (int i = needed_digits - 2; i >= 0; i--) {
    if (X[i] != 0) return false;
  }
  return true;
}

// Returns -1 when the operation would return X unchanged.
int AsUintN_Pos_ResultLength(Digits X, int n) {
  int needed_digits = DIV_CEIL(n, kDigitBits);
  if (X.len() < needed_digits) return -1;
  if (X.len() > needed_digits) return needed_digits;
  int bits_in_top_digit = n % kDigitBits;
  if (bits_in_top_digit == 0) return -1;
  digit_t top_digit = X[needed_digits - 1];
  if ((top_digit >> bits_in_top_digit) == 0) return -1;
  return needed_digits;
}

void AsUintN_Pos(RWDigits Z, Digits X, int n) {
  DCHECK(AsUintN_Pos_ResultLength(X, n) > 0);
  TruncateToNBits(Z, X, n);
}

void AsUintN_Neg(RWDigits Z, Digits X, int n) {
  TruncateAndSubFromPowerOfTwo(Z, X, n);
}

}  // namespace bigint
}  // namespace v8
