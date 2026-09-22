//===-- Implementation header for atanpif16 ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ATANPIF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ATANPIF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "hdr/fenv_macros.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

// Using Python's SymPy library, we can obtain the polynomial approximation of
// arctan(x)/pi. The steps are as follows:
//  >>> from sympy import *
//  >>> import math
//  >>> x = symbols('x')
//  >>> print(series(atan(x)/math.pi, x, 0, 17))
//
// Output:
// 0.318309886183791*x - 0.106103295394597*x**3 + 0.0636619772367581*x**5 -
// 0.0454728408833987*x**7 + 0.0353677651315323*x**9 - 0.0289372623803446*x**11
// + 0.0244853758602916*x**13 - 0.0212206590789194*x**15 + O(x**17)
//
// We will assign this degree-15 Taylor polynomial as g(x). This polynomial
// approximation is accurate for arctan(x)/pi when |x| is in the range [0, 0.5].
//
//
// To compute arctan(x) for all real x, we divide the domain into the following
// cases:
//
// * Case 1: |x| <= 0.5
//      In this range, the direct polynomial approximation is used:
//      arctan(x)/pi = sign(x) * g(|x|)
//      or equivalently, arctan(x) = sign(x) * pi * g(|x|).
//
// * Case 2: 0.5 < |x| <= 1
//      We use the double-angle identity for the tangent function, specifically:
//        arctan(x) = 2 * arctan(x / (1 + sqrt(1 + x^2))).
//      Applying this, we have:
//        arctan(x)/pi = sign(x) * 2 * arctan(x')/pi,
//        where x' = |x| / (1 + sqrt(1 + x^2)).
//        Thus, arctan(x)/pi = sign(x) * 2 * g(x')
//
//      When |x| is in (0.5, 1], the value of x' will always fall within the
//      interval [0.207, 0.414], which is within the accurate range of g(x).
//
// * Case 3: |x| > 1
//      For values of |x| greater than 1, we use the reciprocal transformation
//      identity:
//        arctan(x) = pi/2 - arctan(1/x) for x > 0.
//      For any x (real number), this generalizes to:
//        arctan(x)/pi = sign(x) * (1/2 - arctan(1/|x|)/pi).
//      Then, using g(x) for arctan(1/|x|)/pi:
//        arctan(x)/pi = sign(x) * (1/2 - g(1/|x|)).
//
//      Note that if 1/|x| still falls outside the
//      g(x)'s primary range of accuracy (i.e., if 0.5 < 1/|x| <= 1), the rule
//      from Case 2 must be applied recursively to 1/|x|.

LIBC_INLINE float16 atanpif16(float16 x) {
  using FPBits = fputil::FPBits<float16>;

  FPBits xbits(x);
  bool is_neg = xbits.is_neg();

  auto signed_result = [is_neg](double r) -> float16 {
    return fputil::cast<float16>(is_neg ? -r : r);
  };

  if (LIBC_UNLIKELY(xbits.is_inf_or_nan())) {
    if (xbits.is_nan()) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      return x;
    }
    // atanpi(+-inf) = +-0.5
    return signed_result(0.5);
  }

  if (LIBC_UNLIKELY(xbits.is_zero()))
    return x;

  double x_abs = fputil::cast<double>(xbits.abs().get_val());

  if (LIBC_UNLIKELY(x_abs == 1.0))
    return signed_result(0.25);

  // evaluate atan(x)/pi using polynomial approximation, valid for |x| <= 0.5
  constexpr auto atanpi_eval = [](double x) -> double {
    // polynomial coefficients for atan(x)/pi taylor series
    // generated using sympy: series(atan(x)/pi, x, 0, 17)
    constexpr static double POLY_COEFFS[] = {
        0x1.45f306dc9c889p-2,  // x^1:   1/pi
        -0x1.b2995e7b7b60bp-4, // x^3:  -1/(3*pi)
        0x1.04c26be3b06ccp-4,  // x^5:   1/(5*pi)
        -0x1.7483758e69c08p-5, // x^7:  -1/(7*pi)
        0x1.21bb945252403p-5,  // x^9:   1/(9*pi)
        -0x1.da1bace3cc68ep-6, // x^11: -1/(11*pi)
        0x1.912b1c2336cf2p-6,  // x^13:  1/(13*pi)
        -0x1.5bade52f95e7p-6,  // x^15: -1/(15*pi)
    };
    double x_sq = x * x;
    return x * fputil::polyeval(x_sq, POLY_COEFFS[0], POLY_COEFFS[1],
                                POLY_COEFFS[2], POLY_COEFFS[3], POLY_COEFFS[4],
                                POLY_COEFFS[5], POLY_COEFFS[6], POLY_COEFFS[7]);
  };

  // case 1: |x| <= 0.5 - direct polynomial evaluation
  if (LIBC_LIKELY(x_abs <= 0.5)) {
    double result = atanpi_eval(x_abs);
    return signed_result(result);
  }

  // case 2: 0.5 < |x| <= 1 - use double-angle reduction
  // atan(x) = 2 * atan(x / (1 + sqrt(1 + x^2)))
  // so atanpi(x) = 2 * atanpi(x') where x' = x / (1 + sqrt(1 + x^2))
  if (x_abs <= 1.0) {
    double x_abs_sq = x_abs * x_abs;
    double sqrt_term = fputil::sqrt<double>(1.0 + x_abs_sq);
    double x_prime = x_abs / (1.0 + sqrt_term);
    double result = 2.0 * atanpi_eval(x_prime);
    return signed_result(result);
  }

  // case 3: |x| > 1 - use reciprocal transformation
  // atan(x) = pi/2 - atan(1/x) for x > 0
  // so atanpi(x) = 1/2 - atanpi(1/x)
  double x_recip = 1.0 / x_abs;
  double result = 0.0;

  // if 1/|x| > 0.5, we need to apply Case 2 transformation to 1/|x|
  if (x_recip > 0.5) {
    double x_recip_sq = x_recip * x_recip;
    double sqrt_term = fputil::sqrt<double>(1.0 + x_recip_sq);
    double x_prime = x_recip / (1.0 + sqrt_term);
    result = fputil::multiply_add(-2.0, atanpi_eval(x_prime), 0.5);
  } else {
    // direct evaluation since 1/|x| <= 0.5
    result = 0.5 - atanpi_eval(x_recip);
  }

  return signed_result(result);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ATANPIF16_H
