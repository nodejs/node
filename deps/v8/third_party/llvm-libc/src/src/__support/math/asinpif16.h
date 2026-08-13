//===-- Implementation header for asinpif16 ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_ASINPIF16_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_ASINPIF16_H

#include "include/llvm-libc-macros/float16-macros.h"

#ifdef LIBC_TYPES_HAS_FLOAT16

#include "hdr/errno_macros.h"
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

LIBC_INLINE float16 asinpif16(float16 x) {
  using FPBits = fputil::FPBits<float16>;

  FPBits xbits(x);
  bool is_neg = xbits.is_neg();
  double x_abs = fputil::cast<double>(xbits.abs().get_val());

  auto signed_result = [is_neg](auto r) -> auto { return is_neg ? -r : r; };

  if (LIBC_UNLIKELY(x_abs > 1.0)) {
    // aspinf16(NaN) = NaN
    if (xbits.is_nan()) {
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      return x;
    }

    // 1 < |x| <= +/-inf
    fputil::raise_except_if_required(FE_INVALID);
    fputil::set_errno_if_required(EDOM);

    return FPBits::quiet_nan().get_val();
  }

  // the coefficients for the polynomial approximation of asin(x)/pi in the
  // range [0, 0.5] extracted using python-sympy
  //
  // Python code to generate the coefficients:
  //  > from sympy import *
  //  > import math
  //  > x = symbols('x')
  //  > print(series(asin(x)/math.pi, x, 0, 21))
  //
  // OUTPUT:
  //
  // 0.318309886183791*x + 0.0530516476972984*x**3 + 0.0238732414637843*x**5 +
  // 0.0142102627760621*x**7 + 0.00967087327815336*x**9 +
  // 0.00712127941391293*x**11 + 0.00552355646848375*x**13 +
  // 0.00444514782463692*x**15 + 0.00367705242846804*x**17 +
  // 0.00310721681820837*x**19 + O(x**21)
  //
  // it's very accurate in the range [0, 0.5] and has a maximum error of
  // 0.0000000000000001 in the range [0, 0.5].
  constexpr double POLY_COEFFS[] = {
      0x1.45f306dc9c889p-2, // x^1
      0x1.b2995e7b7b5fdp-5, // x^3
      0x1.8723a1d588a36p-6, // x^5
      0x1.d1a452f20430dp-7, // x^7
      0x1.3ce52a3a09f61p-7, // x^9
      0x1.d2b33e303d375p-8, // x^11
      0x1.69fde663c674fp-8, // x^13
      0x1.235134885f19bp-8, // x^15
  };
  // polynomial evaluation using horner's method
  // work only for |x| in [0, 0.5]
  auto asinpi_polyeval = [&](double x) -> double {
    return x * fputil::polyeval(x * x, POLY_COEFFS[0], POLY_COEFFS[1],
                                POLY_COEFFS[2], POLY_COEFFS[3], POLY_COEFFS[4],
                                POLY_COEFFS[5], POLY_COEFFS[6], POLY_COEFFS[7]);
  };

  // if |x| <= 0.5:
  if (LIBC_UNLIKELY(x_abs <= 0.5)) {
    // Use polynomial approximation of asin(x)/pi in the range [0, 0.5]
    double result = asinpi_polyeval(fputil::cast<double>(x));
    return fputil::cast<float16>(result);
  }

  // If |x| > 0.5, we need to use the range reduction method:
  //    y = asin(x) => x = sin(y)
  //      because: sin(a) = cos(pi/2 - a)
  //      therefore:
  //    x = cos(pi/2 - y)
  //      let z = pi/2 - y,
  //    x = cos(z)
  //      because: cos(2a) = 1 - 2 * sin^2(a), z = 2a, a = z/2
  //      therefore:
  //    cos(z) = 1 - 2 * sin^2(z/2)
  //    sin(z/2) = sqrt((1 - cos(z))/2)
  //    sin(z/2) = sqrt((1 - x)/2)
  //      let u = (1 - x)/2
  //      then:
  //    sin(z/2) = sqrt(u)
  //    z/2 = asin(sqrt(u))
  //    z = 2 * asin(sqrt(u))
  //    pi/2 - y = 2 * asin(sqrt(u))
  //    y = pi/2 - 2 * asin(sqrt(u))
  //    y/pi = 1/2 - 2 * asin(sqrt(u))/pi
  //
  // Finally, we can write:
  //   asinpi(x) = 1/2 - 2 * asinpi(sqrt(u))
  //     where u = (1 - x) /2
  //             = 0.5 - 0.5 * x
  //             = multiply_add(-0.5, x, 0.5)

  double u = fputil::multiply_add(-0.5, x_abs, 0.5);
  double asinpi_sqrt_u = asinpi_polyeval(fputil::sqrt<double>(u));
  double result = fputil::multiply_add(-2.0, asinpi_sqrt_u, 0.5);

  return fputil::cast<float16>(signed_result(result));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT16

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_ASINPIF16_H
