//===-- Single-precision log(x) function ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOGF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOGF_H

#include "common_constants.h" // Lookup table for (1/f) and log(f)
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/macros/properties/cpu_features.h"

// This is an algorithm for log(x) in single precision which is correctly
// rounded for all rounding modes, based on the implementation of log(x) from
// the RLIBM project at:
// https://people.cs.rutgers.edu/~sn349/rlibm

// Step 1 - Range reduction:
//   For x = 2^m * 1.mant, log(x) = m * log(2) + log(1.m)
//   If x is denormal, we normalize it by multiplying x by 2^23 and subtracting
//   m by 23.

// Step 2 - Another range reduction:
//   To compute log(1.mant), let f be the highest 8 bits including the hidden
// bit, and d be the difference (1.mant - f), i.e. the remaining 16 bits of the
// mantissa. Then we have the following approximation formula:
//   log(1.mant) = log(f) + log(1.mant / f)
//               = log(f) + log(1 + d/f)
//               ~ log(f) + P(d/f)
// since d/f is sufficiently small.
//   log(f) and 1/f are then stored in two 2^7 = 128 entries look-up tables.

// Step 3 - Polynomial approximation:
//   To compute P(d/f), we use a single degree-5 polynomial in double precision
// which provides correct rounding for all but few exception values.
//   For more detail about how this polynomial is obtained, please refer to the
// paper:
//   Lim, J. and Nagarakatte, S., "One Polynomial Approximation to Produce
// Correctly Rounded Results of an Elementary Function for Multiple
// Representations and Rounding Modes", Proceedings of the 49th ACM SIGPLAN
// Symposium on Principles of Programming Languages (POPL-2022), Philadelphia,
// USA, January 16-22, 2022.
// https://people.cs.rutgers.edu/~sn349/papers/rlibmall-popl-2022.pdf

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE float logf(float x) {
  using namespace common_constants_internal;
  constexpr double LOG_2 = 0x1.62e42fefa39efp-1;
  using FPBits = typename fputil::FPBits<float>;

  FPBits xbits(x);
  uint32_t x_u = xbits.uintval();

  int m = -FPBits::EXP_BIAS;

  using fputil::round_result_slightly_down;
  using fputil::round_result_slightly_up;

  // Small inputs
  if (x_u < 0x4c5d65a5U) {
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    // Hard-to-round cases.
    switch (x_u) {
    case 0x3f7f4d6fU: // x = 0x1.fe9adep-1f
      return round_result_slightly_up(-0x1.659ec8p-9f);
    case 0x41178febU: // x = 0x1.2f1fd6p+3f
      return round_result_slightly_up(0x1.1fcbcep+1f);
#ifdef LIBC_TARGET_CPU_HAS_FMA
    case 0x3f800000U: // x = 1.0f
      return 0.0f;
#else
    case 0x1e88452dU: // x = 0x1.108a5ap-66f
      return round_result_slightly_up(-0x1.6d7b18p+5f);
#endif // LIBC_TARGET_CPU_HAS_FMA
    }
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    // Subnormal inputs.
    if (LIBC_UNLIKELY(x_u < FPBits::min_normal().uintval())) {
      if (x == 0.0f) {
        // Return -inf and raise FE_DIVBYZERO
        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_DIVBYZERO);
        return FPBits::inf(Sign::NEG).get_val();
      }
      // Normalize denormal inputs.
      xbits = FPBits(xbits.get_val() * 0x1.0p23f);
      m -= 23;
      x_u = xbits.uintval();
    }
  } else {
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    // Hard-to-round cases.
    switch (x_u) {
    case 0x4c5d65a5U: // x = 0x1.bacb4ap+25f
      return round_result_slightly_down(0x1.1e0696p+4f);
    case 0x65d890d3U: // x = 0x1.b121a6p+76f
      return round_result_slightly_down(0x1.a9a3f2p+5f);
    case 0x6f31a8ecU: // x = 0x1.6351d8p+95f
      return round_result_slightly_down(0x1.08b512p+6f);
    case 0x7a17f30aU: // x = 0x1.2fe614p+117f
      return round_result_slightly_up(0x1.451436p+6f);
#ifndef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
    case 0x500ffb03U: // x = 0x1.1ff606p+33f
      return round_result_slightly_up(0x1.6fdd34p+4f);
    case 0x5cd69e88U: // x = 0x1.ad3d1p+58f
      return round_result_slightly_up(0x1.45c146p+5f);
    case 0x5ee8984eU: // x = 0x1.d1309cp+62f;
      return round_result_slightly_up(0x1.5c9442p+5f);
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
    }
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    // Exceptional inputs.
    if (LIBC_UNLIKELY(x_u > FPBits::max_normal().uintval())) {
      if (x_u == 0x8000'0000U) {
        // Return -inf and raise FE_DIVBYZERO
        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_DIVBYZERO);
        return FPBits::inf(Sign::NEG).get_val();
      }
      if (xbits.is_neg() && !xbits.is_nan()) {
        // Return NaN and raise FE_INVALID
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }
      // x is +inf or nan
      if (xbits.is_signaling_nan()) {
        fputil::raise_except_if_required(FE_INVALID);
        return FPBits::quiet_nan().get_val();
      }

      return x;
    }
  }

#ifndef LIBC_TARGET_CPU_HAS_FMA
  // Returning the correct +0 when x = 1.0 for non-FMA targets with FE_DOWNWARD
  // rounding mode.
  if (LIBC_UNLIKELY((x_u & 0x007f'ffffU) == 0))
    return static_cast<float>(
        static_cast<double>(m + xbits.get_biased_exponent()) * LOG_2);
#endif // LIBC_TARGET_CPU_HAS_FMA

  uint32_t mant = xbits.get_mantissa();
  // Extract 7 leading fractional bits of the mantissa
  int index = mant >> 16;
  // Add unbiased exponent. Add an extra 1 if the 7 leading fractional bits are
  // all 1's.
  m += static_cast<int>((x_u + (1 << 16)) >> 23);

  // Set bits to 1.m
  xbits.set_biased_exponent(0x7F);

  float u = xbits.get_val();
  double v = 0.0;
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
  v = static_cast<double>(fputil::multiply_add(u, R[index], -1.0f)); // Exact.
#else
  v = fputil::multiply_add(static_cast<double>(u), RD[index], -1.0); // Exact
#endif // LIBC_TARGET_CPU_HAS_FMA_FLOAT

  // Degree-5 polynomial approximation of log generated by Sollya with:
  // > P = fpminimax(log(1 + x)/x, 4, [|1, D...|], [-2^-8, 2^-7]);
  constexpr double COEFFS[4] = {-0x1.000000000fe63p-1, 0x1.555556e963c16p-2,
                                -0x1.000028dedf986p-2, 0x1.966681bfda7f7p-3};
  double v2 = v * v; // Exact
  double p2 = fputil::multiply_add(v, COEFFS[3], COEFFS[2]);
  double p1 = fputil::multiply_add(v, COEFFS[1], COEFFS[0]);
  double p0 = LOG_R[index] + v;
  double r = fputil::multiply_add(static_cast<double>(m), LOG_2,
                                  fputil::polyeval(v2, p0, p1, p2));
  return static_cast<float>(r);
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOGF_H
