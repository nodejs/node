//===-- Single-precision tanhf function -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_TANHF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_TANHF_H

#include "exp10f_utils.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/macros/properties/cpu_features.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE float tanhf(float x) {
  // 2^6 * log2(e)
  constexpr double LOG2_E_EXP2_6 = ExpBase::LOG2_B * 2.0;

  using FPBits = typename fputil::FPBits<float>;
  FPBits xbits(x);
  uint32_t x_abs = xbits.abs().uintval();

  const int sign_index = xbits.is_neg() ? 1 : 0;

  // When |x| >= 15, or x is inf or nan, or |x| <= 0.078125
  if (LIBC_UNLIKELY((x_abs >= 0x4170'0000U) || (x_abs <= 0x3da0'0000U))) {
    if (x_abs <= 0x3da0'0000U) {
      // |x| <= 0.078125
      if (LIBC_UNLIKELY(x_abs <= 0x3280'0000U)) {
        // |x| <= 2^-26
        return (x_abs != 0)
                   ? static_cast<float>(x - 0x1.5555555555555p-2 * x * x * x)
                   : x;
      }

      const double TAYLOR[] = {-0x1.5555555555555p-2, 0x1.1111111111111p-3,
                               -0x1.ba1ba1ba1ba1cp-5, 0x1.664f4882c10fap-6,
                               -0x1.226e355e6c23dp-7};
      double xdbl = x;
      double x2 = xdbl * xdbl;
      // Taylor polynomial.
      double x4 = x2 * x2;
      double c0 = x2 * TAYLOR[0];
      double c1 = fputil::multiply_add(x2, TAYLOR[2], TAYLOR[1]);
      double c2 = fputil::multiply_add(x2, TAYLOR[4], TAYLOR[3]);
      double pe = fputil::polyeval(x4, c0, c1, c2);

      return static_cast<float>(fputil::multiply_add(xdbl, pe, xdbl));
    }

    // |x| >= 15
    if (LIBC_UNLIKELY(xbits.is_nan()))
      return x + 1.0f; // sNaN to qNaN + signal

    constexpr float SIGNS[2][2] = {{1.0f, -0x1.0p-25f}, {-1.0f, 0x1.0p-25f}};

    if (LIBC_UNLIKELY(xbits.is_inf()))
      return SIGNS[sign_index][0];

    return SIGNS[sign_index][0] + SIGNS[sign_index][1];
  }

  // Range reduction: e^(2x) = 2^(hi + mid) * e^lo
  // Let  k = round( x * 2^6 * log2(e)),
  // So   k  = (hi + mid) * 2^5
  // Then lo = 2x - (hi + mid) * log(2) = 2x - k * 2^-5 * log(2).

  double xd = static_cast<double>(x);
  // k = round( x* 2^6 * log2(e) )
  double k = 0;
  // mk = -k
  int mk = 0;
#ifdef LIBC_TARGET_CPU_HAS_NEAREST_INT
  k = fputil::nearest_integer(xd * LOG2_E_EXP2_6);
  mk = -static_cast<int>(k);
#else
  constexpr double HALF_WAY[2] = {-0.5, 0.5};

  mk = static_cast<int>(
      fputil::multiply_add(xd, -LOG2_E_EXP2_6, HALF_WAY[sign_index]));
  k = static_cast<double>(-mk);
#endif // LIBC_TARGET_CPU_HAS_NEAREST_INT
  // -hi = floor(-k * 2^(-MID_BITS))
  // exp_mhi = shift -hi to the exponent field of double precision.
  int64_t exp_mhi = static_cast<int64_t>(mk >> ExpBase::MID_BITS)
                    << fputil::FPBits<double>::FRACTION_LEN;
  // mh = 2^(-hi - mid)
  int64_t mh_bits = ExpBase::EXP_2_MID[mk & ExpBase::MID_MASK] + exp_mhi;
  double mh = fputil::FPBits<double>(uint64_t(mh_bits)).get_val();
  // dx = lo/2 = x - (hi + mid) * log(2)/2 = x - k * 2^-6 * log(2)
  double dx = fputil::multiply_add(
      k, ExpBase::M_LOGB_2_LO * 0.5,
      fputil::multiply_add(k, ExpBase::M_LOGB_2_HI * 0.5, xd));

  // > P = fpminimax(expm1(2*x)/x, 4, [|D...|], [-log(2)/128, log(2)/128]);
  constexpr double COEFFS[] = {0x1.ffffffffe5bc8p0, 0x1.555555555cd67p0,
                               0x1.5555c2a9b48b4p-1, 0x1.11112a0e34bdbp-2};

  double dx2 = dx * dx;
  double c0 = fputil::multiply_add(dx, 2.0, 1.0);
  double c1 = fputil::multiply_add(dx, COEFFS[1], COEFFS[0]);
  double c2 = fputil::multiply_add(dx, COEFFS[3], COEFFS[2]);
  double r = fputil::polyeval(dx2, c0, c1, c2);

  // tanh(x) = sinh(x) / cosh(x)
  //         = (e^x - e^(-x)) / (e^x + e^(-x))
  //         = (e^(2x) - 1) / (e^(2x) + 1)
  //         = (2^(hi + mid) * e^lo - 1) / (2^(hi + mid) * e^lo + 1)
  //         = (e^lo - 2^(-hi - mid)) / (e^lo + 2^(-hi - mid))
  //         = (r - mh) / (r + mh)
  return static_cast<float>((r - mh) / (r + mh));
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_TANHF_H
