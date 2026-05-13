//===-- Implementation header for atan --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ATAN_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ATAN_H

#include "atan_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

// To compute atan(x), we divided it into the following cases:
// * |x| < 2^-26:
//      Since |x| > atan(|x|) > |x| - |x|^3/3, and |x|^3/3 < ulp(x)/2, we simply
//      return atan(x) = x - sign(x) * epsilon.
// * 2^-26 <= |x| < 1:
//      We perform range reduction mod 2^-6 = 1/64 as follow:
//      Let k = 2^(-6) * round(|x| * 2^6), then
//        atan(x) = sign(x) * atan(|x|)
//                = sign(x) * (atan(k) + atan((|x| - k) / (1 + |x|*k)).
//      We store atan(k) in a look up table, and perform intermediate steps in
//      double-double.
// * 1 < |x| < 2^53:
//      First we perform the transformation y = 1/|x|:
//        atan(x) = sign(x) * (pi/2 - atan(1/|x|))
//                = sign(x) * (pi/2 - atan(y)).
//      Then we compute atan(y) using range reduction mod 2^-6 = 1/64 as the
//      previous case:
//      Let k = 2^(-6) * round(y * 2^6), then
//        atan(y) = atan(k) + atan((y - k) / (1 + y*k))
//                = atan(k) + atan((1/|x| - k) / (1 + k/|x|)
//                = atan(k) + atan((1 - k*|x|) / (|x| + k)).
// * |x| >= 2^53:
//      Using the reciprocal transformation:
//        atan(x) = sign(x) * (pi/2 - atan(1/|x|)).
//      We have that:
//        atan(1/|x|) <= 1/|x| <= 2^-53,
//      which is smaller than ulp(pi/2) / 2.
//      So we can return:
//        atan(x) = sign(x) * (pi/2 - epsilon)

LIBC_INLINE constexpr double atan(double x) {

  using namespace atan_internal;
  using FPBits = fputil::FPBits<double>;

  constexpr double IS_NEG[2] = {1.0, -1.0};
  constexpr DoubleDouble PI_OVER_2 = {0x1.1a62633145c07p-54,
                                      0x1.921fb54442d18p0};
  constexpr DoubleDouble MPI_OVER_2 = {-0x1.1a62633145c07p-54,
                                       -0x1.921fb54442d18p0};

  FPBits xbits(x);
  bool x_sign = xbits.is_neg();
  xbits = xbits.abs();
  uint64_t x_abs = xbits.uintval();
  int x_exp =
      static_cast<int>(x_abs >> FPBits::FRACTION_LEN) - FPBits::EXP_BIAS;

  // |x| < 1.
  if (x_exp < 0) {
    if (LIBC_UNLIKELY(x_exp < -26)) {
#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      return x;
#else
      if (x == 0.0)
        return x;
      // |x| < 2^-26
      return fputil::multiply_add(-0x1.0p-54, x, x);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    }

    double x_d = xbits.get_val();
    // k = 2^-6 * round(2^6 * |x|)
    double k = fputil::nearest_integer(0x1.0p6 * x_d);
    unsigned idx = static_cast<unsigned>(k);
    k *= 0x1.0p-6;

    // numerator = |x| - k
    DoubleDouble num, den;
    num.lo = 0.0;
    num.hi = x_d - k;

    // denominator = 1 - k * |x|
    den.hi = fputil::multiply_add(x_d, k, 1.0);
    DoubleDouble prod = fputil::exact_mult(x_d, k);
    // Using Dekker's 2SUM algorithm to compute the lower part.
    den.lo = ((1.0 - den.hi) + prod.hi) + prod.lo;

    // x_r = (|x| - k) / (1 + k * |x|)
    DoubleDouble x_r = fputil::div(num, den);

    // Approximating atan(x_r) using Taylor polynomial.
    DoubleDouble p = atan_eval(x_r);

    // atan(x) = sign(x) * (atan(k) + atan(x_r))
    //         = sign(x) * (atan(k) + atan( (|x| - k) / (1 + k * |x|) ))
#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    return IS_NEG[x_sign] * (ATAN_I[idx].hi + (p.hi + (p.lo + ATAN_I[idx].lo)));
#else

    DoubleDouble c0 = fputil::exact_add(ATAN_I[idx].hi, p.hi);
    double c1 = c0.lo + (ATAN_I[idx].lo + p.lo);
    double r = IS_NEG[x_sign] * (c0.hi + c1);

    return r;
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  }

  // |x| >= 2^53 or x is NaN.
  if (LIBC_UNLIKELY(x_exp >= 53)) {
    // x is nan
    if (xbits.is_nan()) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      return x;
    }
    // |x| >= 2^53
    // atan(x) ~ sign(x) * pi/2.
    if (x_exp >= 53)
#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
      return IS_NEG[x_sign] * PI_OVER_2.hi;
#else
      return fputil::multiply_add(IS_NEG[x_sign], PI_OVER_2.hi,
                                  IS_NEG[x_sign] * PI_OVER_2.lo);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  }

  double x_d = xbits.get_val();
  double y = 1.0 / x_d;

  // k = 2^-6 * round(2^6 / |x|)
  double k = fputil::nearest_integer(0x1.0p6 * y);
  unsigned idx = static_cast<unsigned>(k);
  k *= 0x1.0p-6;

  // denominator = |x| + k
  DoubleDouble den = fputil::exact_add(x_d, k);
  // numerator = 1 - k * |x|
  DoubleDouble num;
  num.hi = fputil::multiply_add(-x_d, k, 1.0);
  DoubleDouble prod = fputil::exact_mult(x_d, k);
  // Using Dekker's 2SUM algorithm to compute the lower part.
  num.lo = ((1.0 - num.hi) - prod.hi) - prod.lo;

  // x_r = (1/|x| - k) / (1 - k/|x|)
  //     = (1 - k * |x|) / (|x| - k)
  DoubleDouble x_r = fputil::div(num, den);

  // Approximating atan(x_r) using Taylor polynomial.
  DoubleDouble p = atan_eval(x_r);

  // atan(x) = sign(x) * (pi/2 - atan(1/|x|))
  //         = sign(x) * (pi/2 - atan(k) - atan(x_r))
  //         = (-sign(x)) * (-pi/2 + atan(k) + atan((1 - k*|x|)/(|x| - k)))
#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  double lo_part = p.lo + ATAN_I[idx].lo + MPI_OVER_2.lo;
  return IS_NEG[!x_sign] * (MPI_OVER_2.hi + ATAN_I[idx].hi + (p.hi + lo_part));
#else
  DoubleDouble c0 = fputil::exact_add(MPI_OVER_2.hi, ATAN_I[idx].hi);
  DoubleDouble c1 = fputil::exact_add(c0.hi, p.hi);
  double c2 = c1.lo + (c0.lo + p.lo) + (ATAN_I[idx].lo + MPI_OVER_2.lo);

  double r = IS_NEG[!x_sign] * (c1.hi + c2);

  return r;
#endif
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ATAN_H
