//===-- Implementation of hypotf function ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_HYPOT_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_HYPOT_H

#include "BasicOperations.h"
#include "FEnvImpl.h"
#include "FPBits.h"
#include "cast.h"
#include "rounding_mode.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/uint128.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace internal {

template <typename T>
LIBC_INLINE T find_leading_one(T mant, int &shift_length) {
  shift_length = 0;
  if (mant > 0) {
    shift_length = (sizeof(mant) * 8) - 1 - cpp::countl_zero(mant);
  }
  return static_cast<T>((T(1) << shift_length));
}

} // namespace internal

template <typename T> struct DoubleLength;

template <> struct DoubleLength<uint16_t> {
  using Type = uint32_t;
};

template <> struct DoubleLength<uint32_t> {
  using Type = uint64_t;
};

template <> struct DoubleLength<uint64_t> {
  using Type = UInt128;
};

// Correctly rounded IEEE 754 HYPOT(x, y) with round to nearest, ties to even.
//
// Algorithm:
//   -  Let a = max(|x|, |y|), b = min(|x|, |y|), then we have that:
//          a <= sqrt(a^2 + b^2) <= min(a + b, a*sqrt(2))
//   1. So if b < eps(a)/2, then HYPOT(x, y) = a.
//
//   -  Moreover, the exponent part of HYPOT(x, y) is either the same or 1 more
//      than the exponent part of a.
//
//   2. For the remaining cases, we will use the digit-by-digit (shift-and-add)
//      algorithm to compute SQRT(Z):
//
//   -  For Y = y0.y1...yn... = SQRT(Z),
//      let Y(n) = y0.y1...yn be the first n fractional digits of Y.
//
//   -  The nth scaled residual R(n) is defined to be:
//          R(n) = 2^n * (Z - Y(n)^2)
//
//   -  Since Y(n) = Y(n - 1) + yn * 2^(-n), the scaled residual
//      satisfies the following recurrence formula:
//          R(n) = 2*R(n - 1) - yn*(2*Y(n - 1) + 2^(-n)),
//      with the initial conditions:
//          Y(0) = y0, and R(0) = Z - y0.
//
//   -  So the nth fractional digit of Y = SQRT(Z) can be decided by:
//          yn = 1  if 2*R(n - 1) >= 2*Y(n - 1) + 2^(-n),
//               0  otherwise.
//
//   3. Precision analysis:
//
//   -  Notice that in the decision function:
//          2*R(n - 1) >= 2*Y(n - 1) + 2^(-n),
//      the right hand side only uses up to the 2^(-n)-bit, and both sides are
//      non-negative, so R(n - 1) can be truncated at the 2^(-(n + 1))-bit, so
//      that 2*R(n - 1) is corrected up to the 2^(-n)-bit.
//
//   -  Thus, in order to round SQRT(a^2 + b^2) correctly up to n-fractional
//      bits, we need to perform the summation (a^2 + b^2) correctly up to (2n +
//      2)-fractional bits, and the remaining bits are sticky bits (i.e. we only
//      care if they are 0 or > 0), and the comparisons, additions/subtractions
//      can be done in n-fractional bits precision.
//
//   -  For single precision (float), we can use uint64_t to store the sum a^2 +
//      b^2 exact up to (2n + 2)-fractional bits.
//
//   -  Then we can feed this sum into the digit-by-digit algorithm for SQRT(Z)
//      described above.
//
//
// Special cases:
//   - HYPOT(x, y) is +Inf if x or y is +Inf or -Inf; else
//   - HYPOT(x, y) is NaN if x or y is NaN.
//
template <typename T, cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE T hypot(T x, T y) {
  using FPBits_t = FPBits<T>;
  using StorageType = typename FPBits<T>::StorageType;
  using DStorageType = typename DoubleLength<StorageType>::Type;

  FPBits_t x_abs = FPBits_t(x).abs();
  FPBits_t y_abs = FPBits_t(y).abs();

  bool x_abs_larger = x_abs.uintval() >= y_abs.uintval();

  FPBits_t a_bits = x_abs_larger ? x_abs : y_abs;
  FPBits_t b_bits = x_abs_larger ? y_abs : x_abs;

  if (LIBC_UNLIKELY(a_bits.is_inf_or_nan())) {
    if (x_abs.is_signaling_nan() || y_abs.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits_t::quiet_nan().get_val();
    }
    if (x_abs.is_inf() || y_abs.is_inf())
      return FPBits_t::inf().get_val();
    if (x_abs.is_nan())
      return x;
    // y is nan
    return y;
  }

  uint16_t a_exp = a_bits.get_biased_exponent();
  uint16_t b_exp = b_bits.get_biased_exponent();

  if ((a_exp - b_exp >= FPBits_t::FRACTION_LEN + 2) || (x == 0) || (y == 0)) {
#ifdef LIBC_TYPES_HAS_FLOAT16
    if constexpr (cpp::is_same_v<T, float16>) {
      // Compiler runtime for basic operations of float16 might not be correctly
      // rounded for all rounding modes.
      float af = fputil::cast<float>(x_abs.get_val());
      float bf = fputil::cast<float>(y_abs.get_val());
      return fputil::cast<float16>(af + bf);
    } else
#endif // LIBC_TYPES_HAS_FLOAT16
      return x_abs.get_val() + y_abs.get_val();
  }

  uint64_t out_exp = a_exp;
  StorageType a_mant = a_bits.get_mantissa();
  StorageType b_mant = b_bits.get_mantissa();
  DStorageType a_mant_sq, b_mant_sq;
  bool sticky_bits;

  // Add an extra bit to simplify the final rounding bit computation.
  constexpr StorageType ONE = StorageType(1) << (FPBits_t::FRACTION_LEN + 1);

  a_mant <<= 1;
  b_mant <<= 1;

  StorageType leading_one;
  int y_mant_width;
  if (a_exp != 0) {
    leading_one = ONE;
    a_mant |= ONE;
    y_mant_width = FPBits_t::FRACTION_LEN + 1;
  } else {
    leading_one = internal::find_leading_one(a_mant, y_mant_width);
    a_exp = 1;
  }

  if (b_exp != 0)
    b_mant |= ONE;
  else
    b_exp = 1;

  a_mant_sq = static_cast<DStorageType>(a_mant) * a_mant;
  b_mant_sq = static_cast<DStorageType>(b_mant) * b_mant;

  // At this point, a_exp >= b_exp > a_exp - 25, so in order to line up aSqMant
  // and bSqMant, we need to shift bSqMant to the right by (a_exp - b_exp) bits.
  // But before that, remember to store the losing bits to sticky.
  // The shift length is for a^2 and b^2, so it's double of the exponent
  // difference between a and b.
  uint16_t shift_length = static_cast<uint16_t>(2 * (a_exp - b_exp));
  sticky_bits =
      ((b_mant_sq & ((DStorageType(1) << shift_length) - DStorageType(1))) !=
       DStorageType(0));
  b_mant_sq >>= shift_length;

  DStorageType sum = a_mant_sq + b_mant_sq;
  if (sum >= (DStorageType(1) << (2 * y_mant_width + 2))) {
    // a^2 + b^2 >= 4* leading_one^2, so we will need an extra bit to the left.
    if (leading_one == ONE) {
      // For normal result, we discard the last 2 bits of the sum and increase
      // the exponent.
      sticky_bits = sticky_bits || ((sum & 0x3U) != 0);
      sum >>= 2;
      ++out_exp;
      if (out_exp >= FPBits_t::MAX_BIASED_EXPONENT) {
        if (int round_mode = quick_get_round();
            round_mode == FE_TONEAREST || round_mode == FE_UPWARD)
          return FPBits_t::inf().get_val();
        return FPBits_t::max_normal().get_val();
      }
    } else {
      // For denormal result, we simply move the leading bit of the result to
      // the left by 1.
      leading_one <<= 1;
      ++y_mant_width;
    }
  }

  StorageType y_new = leading_one;
  StorageType r = static_cast<StorageType>(sum >> y_mant_width) - leading_one;
  StorageType tail_bits = static_cast<StorageType>(sum) & (leading_one - 1);

  for (StorageType current_bit = leading_one >> 1; current_bit;
       current_bit >>= 1) {
    r = static_cast<StorageType>((r << 1) +
                                 ((tail_bits & current_bit) ? 1 : 0));
    StorageType tmp = static_cast<StorageType>((y_new << 1)) +
                      current_bit; // 2*y_new(n - 1) + 2^(-n)
    if (r >= tmp) {
      r -= tmp;
      y_new += current_bit;
    }
  }

  bool round_bit = y_new & StorageType(1);
  bool lsb = y_new & StorageType(2);

  if (y_new >= ONE) {
    y_new -= ONE;

    if (out_exp == 0) {
      out_exp = 1;
    }
  }

  y_new >>= 1;

  // Round to the nearest, tie to even.
  int round_mode = quick_get_round();
  switch (round_mode) {
  case FE_TONEAREST:
    // Round to nearest, ties to even
    if (round_bit && (lsb || sticky_bits || (r != 0)))
      ++y_new;
    break;
  case FE_UPWARD:
    if (round_bit || sticky_bits || (r != 0))
      ++y_new;
    break;
  }

  if (y_new >= (ONE >> 1)) {
    y_new -= ONE >> 1;
    ++out_exp;
    if (out_exp >= FPBits_t::MAX_BIASED_EXPONENT) {
      if (round_mode == FE_TONEAREST || round_mode == FE_UPWARD)
        return FPBits_t::inf().get_val();
      return FPBits_t::max_normal().get_val();
    }
  }

  y_new |= static_cast<StorageType>(out_exp) << FPBits_t::FRACTION_LEN;

  if (!(round_bit || sticky_bits || (r != 0)))
    fputil::clear_except_if_required(FE_INEXACT);

  return cpp::bit_cast<T>(y_new);
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_HYPOT_H
