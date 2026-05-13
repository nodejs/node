//===-- Common header for FMA implementations -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_FMA_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_FMA_H

#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/big_int.h"
#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

#include "hdr/fenv_macros.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {
namespace generic {

template <typename OutType, typename InType>
LIBC_INLINE cpp::enable_if_t<cpp::is_floating_point_v<OutType> &&
                                 cpp::is_floating_point_v<InType> &&
                                 sizeof(OutType) <= sizeof(InType),
                             OutType>
fma(InType x, InType y, InType z);

// TODO(lntue): Implement fmaf that is correctly rounded to all rounding modes.
// The implementation below only is only correct for the default rounding mode,
// round-to-nearest tie-to-even.
template <> LIBC_INLINE float fma<float>(float x, float y, float z) {
  // Product is exact.
  double prod = static_cast<double>(x) * static_cast<double>(y);
  double z_d = static_cast<double>(z);
  double sum = prod + z_d;
  fputil::FPBits<double> bit_prod(prod), bitz(z_d), bit_sum(sum);

  if (!(bit_sum.is_inf_or_nan() || bit_sum.is_zero())) {
    // Since the sum is computed in double precision, rounding might happen
    // (for instance, when bitz.exponent > bit_prod.exponent + 5, or
    // bit_prod.exponent > bitz.exponent + 40).  In that case, when we round
    // the sum back to float, double rounding error might occur.
    // A concrete example of this phenomenon is as follows:
    //   x = y = 1 + 2^(-12), z = 2^(-53)
    // The exact value of x*y + z is 1 + 2^(-11) + 2^(-24) + 2^(-53)
    // So when rounding to float, fmaf(x, y, z) = 1 + 2^(-11) + 2^(-23)
    // On the other hand, with the default rounding mode,
    //   double(x*y + z) = 1 + 2^(-11) + 2^(-24)
    // and casting again to float gives us:
    //   float(double(x*y + z)) = 1 + 2^(-11).
    //
    // In order to correct this possible double rounding error, first we use
    // Dekker's 2Sum algorithm to find t such that sum - t = prod + z exactly,
    // assuming the (default) rounding mode is round-to-the-nearest,
    // tie-to-even.  Moreover, t satisfies the condition that t < eps(sum),
    // i.e., t.exponent < sum.exponent - 52. So if t is not 0, meaning rounding
    // occurs when computing the sum, we just need to use t to adjust (any) last
    // bit of sum, so that the sticky bits used when rounding sum to float are
    // correct (when it matters).
    fputil::FPBits<double> t(
        (bit_prod.get_biased_exponent() >= bitz.get_biased_exponent())
            ? ((bit_sum.get_val() - bit_prod.get_val()) - bitz.get_val())
            : ((bit_sum.get_val() - bitz.get_val()) - bit_prod.get_val()));

    // Update sticky bits if t != 0.0 and the least (52 - 23 - 1 = 28) bits are
    // zero.
    if (!t.is_zero() && ((bit_sum.get_mantissa() & 0xfff'ffffULL) == 0)) {
      if (bit_sum.sign() != t.sign())
        bit_sum.set_mantissa(bit_sum.get_mantissa() + 1);
      else if (bit_sum.get_mantissa())
        bit_sum.set_mantissa(bit_sum.get_mantissa() - 1);
    }
  }

  return static_cast<float>(bit_sum.get_val());
}

namespace internal {

// Extract the sticky bits and shift the `mantissa` to the right by
// `shift_length`.
template <typename T>
LIBC_INLINE cpp::enable_if_t<is_unsigned_integral_or_big_int_v<T>, bool>
shift_mantissa(int shift_length, T &mant) {
  if (shift_length >= cpp::numeric_limits<T>::digits) {
    mant = 0;
    return true; // prod_mant is non-zero.
  }
  T mask = (T(1) << shift_length) - 1;
  bool sticky_bits = (mant & mask) != 0;
  mant >>= shift_length;
  return sticky_bits;
}

} // namespace internal

template <typename OutType, typename InType>
LIBC_INLINE cpp::enable_if_t<cpp::is_floating_point_v<OutType> &&
                                 cpp::is_floating_point_v<InType> &&
                                 sizeof(OutType) <= sizeof(InType),
                             OutType>
fma(InType x, InType y, InType z) {
  using OutFPBits = FPBits<OutType>;
  using OutStorageType = typename OutFPBits::StorageType;
  using InFPBits = FPBits<InType>;
  using InStorageType = typename InFPBits::StorageType;

  constexpr int IN_EXPLICIT_MANT_LEN = InFPBits::FRACTION_LEN + 1;
  constexpr size_t PROD_LEN = 2 * IN_EXPLICIT_MANT_LEN;
  constexpr size_t TMP_RESULT_LEN = cpp::bit_ceil(PROD_LEN + 1);
  using TmpResultType = UInt<TMP_RESULT_LEN>;
  using DyadicFloat = DyadicFloat<TMP_RESULT_LEN>;

  InFPBits x_bits(x), y_bits(y), z_bits(z);

  if (LIBC_UNLIKELY(x_bits.is_nan() || y_bits.is_nan() || z_bits.is_nan())) {
    if (x_bits.is_nan() || y_bits.is_nan()) {
      if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan() ||
          z_bits.is_signaling_nan())
        raise_except_if_required(FE_INVALID);

      if (x_bits.is_quiet_nan()) {
        InStorageType x_payload = x_bits.get_mantissa();
        x_payload >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(x_bits.sign(),
                                    static_cast<OutStorageType>(x_payload))
            .get_val();
      }

      if (y_bits.is_quiet_nan()) {
        InStorageType y_payload = y_bits.get_mantissa();
        y_payload >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(y_bits.sign(),
                                    static_cast<OutStorageType>(y_payload))
            .get_val();
      }

      if (z_bits.is_quiet_nan()) {
        InStorageType z_payload = z_bits.get_mantissa();
        z_payload >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(z_bits.sign(),
                                    static_cast<OutStorageType>(z_payload))
            .get_val();
      }

      return OutFPBits::quiet_nan().get_val();
    }
  }

  if (LIBC_UNLIKELY(x == 0 || y == 0 || z == 0))
    return cast<OutType>(x * y + z);

  int x_exp = 0;
  int y_exp = 0;
  int z_exp = 0;

  // Denormal scaling = 2^(fraction length).
  constexpr InStorageType IMPLICIT_MASK =
      InFPBits::SIG_MASK - InFPBits::FRACTION_MASK;

  constexpr InType DENORMAL_SCALING =
      InFPBits::create_value(
          Sign::POS, InFPBits::FRACTION_LEN + InFPBits::EXP_BIAS, IMPLICIT_MASK)
          .get_val();

  // Normalize denormal inputs.
  if (LIBC_UNLIKELY(InFPBits(x).is_subnormal())) {
    x_exp -= InFPBits::FRACTION_LEN;
    x *= DENORMAL_SCALING;
  }
  if (LIBC_UNLIKELY(InFPBits(y).is_subnormal())) {
    y_exp -= InFPBits::FRACTION_LEN;
    y *= DENORMAL_SCALING;
  }
  if (LIBC_UNLIKELY(InFPBits(z).is_subnormal())) {
    z_exp -= InFPBits::FRACTION_LEN;
    z *= DENORMAL_SCALING;
  }

  x_bits = InFPBits(x);
  y_bits = InFPBits(y);
  z_bits = InFPBits(z);
  const Sign z_sign = z_bits.sign();
  Sign prod_sign = (x_bits.sign() == y_bits.sign()) ? Sign::POS : Sign::NEG;
  x_exp += x_bits.get_biased_exponent();
  y_exp += y_bits.get_biased_exponent();
  z_exp += z_bits.get_biased_exponent();

  if (LIBC_UNLIKELY(x_exp == InFPBits::MAX_BIASED_EXPONENT ||
                    y_exp == InFPBits::MAX_BIASED_EXPONENT ||
                    z_exp == InFPBits::MAX_BIASED_EXPONENT)) {
    if (LIBC_UNLIKELY(x_exp != InFPBits::MAX_BIASED_EXPONENT &&
                      y_exp != InFPBits::MAX_BIASED_EXPONENT &&
                      z_bits.is_inf()))
      return cast<OutType>(z);
    return cast<OutType>(x * y + z);
  }

  // Extract mantissa and append hidden leading bits.
  InStorageType x_mant = x_bits.get_explicit_mantissa();
  InStorageType y_mant = y_bits.get_explicit_mantissa();
  TmpResultType z_mant = z_bits.get_explicit_mantissa();

  // If the exponent of the product x*y > the exponent of z, then no extra
  // precision beside the entire product x*y is needed.  On the other hand, when
  // the exponent of z >= the exponent of the product x*y, the worst-case that
  // we need extra precision is when there is cancellation and the most
  // significant bit of the product is aligned exactly with the second most
  // significant bit of z:
  //      z :    10aa...a
  // - prod :     1bb...bb....b
  // In that case, in order to store the exact result, we need at least
  //     (Length of prod) - (Fraction length of z)
  //   = 2*(Length of input explicit mantissa) - (Fraction length of z) bits.
  // Overall, before aligning the mantissas and exponents, we can simply left-
  // shift the mantissa of z by that amount.  After that, it is enough to align
  // the least significant bit, given that we keep track of the round and sticky
  // bits after the least significant bit.

  TmpResultType prod_mant = TmpResultType(x_mant) * y_mant;
  int prod_lsb_exp =
      x_exp + y_exp - (InFPBits::EXP_BIAS + 2 * InFPBits::FRACTION_LEN);

  constexpr int RESULT_MIN_LEN = PROD_LEN - InFPBits::FRACTION_LEN;
  z_mant <<= RESULT_MIN_LEN;
  int z_lsb_exp = z_exp - (InFPBits::FRACTION_LEN + RESULT_MIN_LEN);
  bool sticky_bits = false;
  bool z_shifted = false;

  // Align exponents.
  if (prod_lsb_exp < z_lsb_exp) {
    sticky_bits = internal::shift_mantissa(z_lsb_exp - prod_lsb_exp, prod_mant);
    prod_lsb_exp = z_lsb_exp;
  } else if (z_lsb_exp < prod_lsb_exp) {
    z_shifted = true;
    sticky_bits = internal::shift_mantissa(prod_lsb_exp - z_lsb_exp, z_mant);
  }

  // Perform the addition:
  //   (-1)^prod_sign * prod_mant + (-1)^z_sign * z_mant.
  // The final result will be stored in prod_sign and prod_mant.
  if (prod_sign == z_sign) {
    // Effectively an addition.
    prod_mant += z_mant;
  } else {
    // Subtraction cases.
    if (prod_mant >= z_mant) {
      if (z_shifted && sticky_bits) {
        // Add 1 more to the subtrahend so that the sticky bits remain
        // positive. This would simplify the rounding logic.
        ++z_mant;
      }
      prod_mant -= z_mant;
    } else {
      if (!z_shifted && sticky_bits) {
        // Add 1 more to the subtrahend so that the sticky bits remain
        // positive. This would simplify the rounding logic.
        ++prod_mant;
      }
      prod_mant = z_mant - prod_mant;
      prod_sign = z_sign;
    }
  }

  if (prod_mant == 0) {
    // When there is exact cancellation, i.e., x*y == -z exactly, return -0.0 if
    // rounding downward and +0.0 for other rounding modes.
    if (fputil::quick_get_round() == FE_DOWNWARD)
      prod_sign = Sign::NEG;
    else
      prod_sign = Sign::POS;
  }

  DyadicFloat result(prod_sign, prod_lsb_exp - InFPBits::EXP_BIAS, prod_mant);
  result.mantissa |= static_cast<unsigned int>(sticky_bits);
  return result.template as<OutType, /*ShouldSignalExceptions=*/true>();
}

} // namespace generic
} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_FMA_H
