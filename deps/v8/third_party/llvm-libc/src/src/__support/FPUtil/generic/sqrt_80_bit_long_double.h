//===-- Square root of x86 long double numbers ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_SQRT_80_BIT_LONG_DOUBLE_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_SQRT_80_BIT_LONG_DOUBLE_H

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/uint128.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {
namespace x86 {

LIBC_INLINE void normalize(int &exponent,
                           FPBits<long double>::StorageType &mantissa) {
  const unsigned int shift = static_cast<unsigned int>(
      static_cast<size_t>(cpp::countl_zero(static_cast<uint64_t>(mantissa))) -
      (8 * sizeof(uint64_t) - 1 - FPBits<long double>::FRACTION_LEN));
  exponent -= shift;
  mantissa <<= shift;
}

// if constexpr statement in sqrt.h still requires x86::sqrt to be declared
// even when it's not used.
LIBC_INLINE long double sqrt(long double x);

// Correctly rounded SQRT for all rounding modes.
// Shift-and-add algorithm.
#if defined(LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80)
LIBC_INLINE long double sqrt(long double x) {
  using LDBits = FPBits<long double>;
  using StorageType = typename LDBits::StorageType;
  constexpr StorageType ONE = StorageType(1) << int(LDBits::FRACTION_LEN);
  constexpr auto LDNAN = LDBits::quiet_nan().get_val();

  LDBits bits(x);

  if (bits == LDBits::inf(Sign::POS) || bits.is_zero() || bits.is_nan()) {
    // sqrt(+Inf) = +Inf
    // sqrt(+0) = +0
    // sqrt(-0) = -0
    // sqrt(NaN) = NaN
    // sqrt(-NaN) = -NaN
    return x;
  } else if (bits.is_neg()) {
    // sqrt(-Inf) = NaN
    // sqrt(-x) = NaN
    return LDNAN;
  } else {
    int x_exp = bits.get_explicit_exponent();
    StorageType x_mant = bits.get_mantissa();

    // Step 1a: Normalize denormal input
    if (bits.get_implicit_bit()) {
      x_mant |= ONE;
    } else if (bits.is_subnormal()) {
      normalize(x_exp, x_mant);
    }

    // Step 1b: Make sure the exponent is even.
    if (x_exp & 1) {
      --x_exp;
      x_mant <<= 1;
    }

    // After step 1b, x = 2^(x_exp) * x_mant, where x_exp is even, and
    // 1 <= x_mant < 4.  So sqrt(x) = 2^(x_exp / 2) * y, with 1 <= y < 2.
    // Notice that the output of sqrt is always in the normal range.
    // To perform shift-and-add algorithm to find y, let denote:
    //   y(n) = 1.y_1 y_2 ... y_n, we can define the nth residue to be:
    //   r(n) = 2^n ( x_mant - y(n)^2 ).
    // That leads to the following recurrence formula:
    //   r(n) = 2*r(n-1) - y_n*[ 2*y(n-1) + 2^(-n-1) ]
    // with the initial conditions: y(0) = 1, and r(0) = x - 1.
    // So the nth digit y_n of the mantissa of sqrt(x) can be found by:
    //   y_n = 1 if 2*r(n-1) >= 2*y(n - 1) + 2^(-n-1)
    //         0 otherwise.
    StorageType y = ONE;
    StorageType r = x_mant - ONE;

    for (StorageType current_bit = ONE >> 1; current_bit; current_bit >>= 1) {
      r <<= 1;
      StorageType tmp = (y << 1) + current_bit; // 2*y(n - 1) + 2^(-n-1)
      if (r >= tmp) {
        r -= tmp;
        y += current_bit;
      }
    }

    // We compute one more iteration in order to round correctly.
    bool lsb = static_cast<bool>(y & 1); // Least significant bit
    bool rb = false;                     // Round bit
    r <<= 2;
    StorageType tmp = (y << 2) + 1;
    if (r >= tmp) {
      r -= tmp;
      rb = true;
    }

    // Append the exponent field.
    x_exp = ((x_exp >> 1) + LDBits::EXP_BIAS);
    y |= (static_cast<StorageType>(x_exp) << (LDBits::FRACTION_LEN + 1));

    switch (quick_get_round()) {
    case FE_TONEAREST:
      // Round to nearest, ties to even
      if (rb && (lsb || (r != 0)))
        ++y;
      break;
    case FE_UPWARD:
      if (rb || (r != 0))
        ++y;
      break;
    }

    // Extract output
    FPBits<long double> out(0.0L);
    out.set_biased_exponent(x_exp);
    out.set_implicit_bit(1);
    out.set_mantissa((y & (ONE - 1)));

    return out.get_val();
  }
}
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

} // namespace x86
} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_SQRT_80_BIT_LONG_DOUBLE_H
