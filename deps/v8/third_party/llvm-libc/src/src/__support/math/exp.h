//===-- Implementation header for exp ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP_H

#include "exp_constants.h"
#include "exp_utils.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/FPUtil/triple_double.h"
#include "src/__support/common.h"
#include "src/__support/integer_literals.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace exp_internal {

using fputil::DoubleDouble;
using fputil::TripleDouble;
using Float128 = typename fputil::DyadicFloat<128>;

using LIBC_NAMESPACE::operator""_u128;

// log2(e)
LIBC_INLINE_VAR constexpr double LOG2_E = 0x1.71547652b82fep+0;

// Error bounds:
// Errors when using double precision.
LIBC_INLINE_VAR constexpr double EXP_ERR_D = 0x1.8p-63;

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
// Errors when using double-double precision.
LIBC_INLINE_VAR constexpr double EXP_ERR_DD = 0x1.0p-99;
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

// -2^-12 * log(2)
// > a = -2^-12 * log(2);
// > b = round(a, 30, RN);
// > c = round(a - b, 30, RN);
// > d = round(a - b - c, D, RN);
// Errors < 1.5 * 2^-133
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_HI = -0x1.62e42ffp-13;
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_MID = 0x1.718432a1b0e26p-47;

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_MID_30 = 0x1.718432ap-47;
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_LO = 0x1.b0e2633fe0685p-79;
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

// Polynomial approximations with double precision:
// Return expm1(dx) / x ~ 1 + dx / 2 + dx^2 / 6 + dx^3 / 24.
// For |dx| < 2^-13 + 2^-30:
//   | output - expm1(dx) / dx | < 2^-51.
LIBC_INLINE double poly_approx_d(double dx) {
  // dx^2
  double dx2 = dx * dx;
  // c0 = 1 + dx / 2
  double c0 = fputil::multiply_add(dx, 0.5, 1.0);
  // c1 = 1/6 + dx / 24
  double c1 =
      fputil::multiply_add(dx, 0x1.5555555555555p-5, 0x1.5555555555555p-3);
  // p = dx^2 * c1 + c0 = 1 + dx / 2 + dx^2 / 6 + dx^3 / 24
  double p = fputil::multiply_add(dx2, c1, c0);
  return p;
}

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
// Polynomial approximation with double-double precision:
// Return exp(dx) ~ 1 + dx + dx^2 / 2 + ... + dx^6 / 720
// For |dx| < 2^-13 + 2^-30:
//   | output - exp(dx) | < 2^-101
LIBC_INLINE DoubleDouble poly_approx_dd(const DoubleDouble &dx) {
  // Taylor polynomial.
  constexpr DoubleDouble COEFFS[] = {
      {0, 0x1p0},                                      // 1
      {0, 0x1p0},                                      // 1
      {0, 0x1p-1},                                     // 1/2
      {0x1.5555555555555p-57, 0x1.5555555555555p-3},   // 1/6
      {0x1.5555555555555p-59, 0x1.5555555555555p-5},   // 1/24
      {0x1.1111111111111p-63, 0x1.1111111111111p-7},   // 1/120
      {-0x1.f49f49f49f49fp-65, 0x1.6c16c16c16c17p-10}, // 1/720
  };

  DoubleDouble p = fputil::polyeval(dx, COEFFS[0], COEFFS[1], COEFFS[2],
                                    COEFFS[3], COEFFS[4], COEFFS[5], COEFFS[6]);
  return p;
}

// Polynomial approximation with 128-bit precision:
// Return exp(dx) ~ 1 + dx + dx^2 / 2 + ... + dx^7 / 5040
// For |dx| < 2^-13 + 2^-30:
//   | output - exp(dx) | < 2^-126.
LIBC_INLINE Float128 poly_approx_f128(const Float128 &dx) {
  constexpr Float128 COEFFS_128[]{
      {Sign::POS, -127, 0x80000000'00000000'00000000'00000000_u128}, // 1.0
      {Sign::POS, -127, 0x80000000'00000000'00000000'00000000_u128}, // 1.0
      {Sign::POS, -128, 0x80000000'00000000'00000000'00000000_u128}, // 0.5
      {Sign::POS, -130, 0xaaaaaaaa'aaaaaaaa'aaaaaaaa'aaaaaaab_u128}, // 1/6
      {Sign::POS, -132, 0xaaaaaaaa'aaaaaaaa'aaaaaaaa'aaaaaaab_u128}, // 1/24
      {Sign::POS, -134, 0x88888888'88888888'88888888'88888889_u128}, // 1/120
      {Sign::POS, -137, 0xb60b60b6'0b60b60b'60b60b60'b60b60b6_u128}, // 1/720
      {Sign::POS, -140, 0xd00d00d0'0d00d00d'00d00d00'd00d00d0_u128}, // 1/5040
  };

  Float128 p = fputil::polyeval(dx, COEFFS_128[0], COEFFS_128[1], COEFFS_128[2],
                                COEFFS_128[3], COEFFS_128[4], COEFFS_128[5],
                                COEFFS_128[6], COEFFS_128[7]);
  return p;
}

// Compute exp(x) using 128-bit precision.
// TODO(lntue): investigate triple-double precision implementation for this
// step.
LIBC_INLINE Float128 exp_f128(double x, double kd, int idx1, int idx2) {
  // Recalculate dx:

  double t1 = fputil::multiply_add(kd, MLOG_2_EXP2_M12_HI, x); // exact
  double t2 = kd * MLOG_2_EXP2_M12_MID_30;                     // exact
  double t3 = kd * MLOG_2_EXP2_M12_LO;                         // Error < 2^-133

  Float128 dx = fputil::quick_add(
      Float128(t1), fputil::quick_add(Float128(t2), Float128(t3)));

  // TODO: Skip recalculating exp_mid1 and exp_mid2.
  Float128 exp_mid1 =
      fputil::quick_add(Float128(EXP2_MID1[idx1].hi),
                        fputil::quick_add(Float128(EXP2_MID1[idx1].mid),
                                          Float128(EXP2_MID1[idx1].lo)));

  Float128 exp_mid2 =
      fputil::quick_add(Float128(EXP2_MID2[idx2].hi),
                        fputil::quick_add(Float128(EXP2_MID2[idx2].mid),
                                          Float128(EXP2_MID2[idx2].lo)));

  Float128 exp_mid = fputil::quick_mul(exp_mid1, exp_mid2);

  Float128 p = poly_approx_f128(dx);

  Float128 r = fputil::quick_mul(exp_mid, p);

  r.exponent += static_cast<int>(kd) >> 12;

  return r;
}

// Compute exp(x) with double-double precision.
LIBC_INLINE DoubleDouble exp_double_double(double x, double kd,
                                           const DoubleDouble &exp_mid) {
  // Recalculate dx:
  //   dx = x - k * 2^-12 * log(2)
  double t1 = fputil::multiply_add(kd, MLOG_2_EXP2_M12_HI, x); // exact
  double t2 = kd * MLOG_2_EXP2_M12_MID_30;                     // exact
  double t3 = kd * MLOG_2_EXP2_M12_LO;                         // Error < 2^-130

  DoubleDouble dx = fputil::exact_add(t1, t2);
  dx.lo += t3;

  // Degree-6 Taylor polynomial approximation in double-double precision.
  // | p - exp(x) | < 2^-100.
  DoubleDouble p = poly_approx_dd(dx);

  // Error bounds: 2^-99.
  DoubleDouble r = fputil::quick_mult(exp_mid, p);

  return r;
}
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

// Check for exceptional cases when
// |x| <= 2^-53 or x < log(2^-1075) or x >= 0x1.6232bdd7abcd3p+9
LIBC_INLINE double set_exceptional(double x) {
  using FPBits = typename fputil::FPBits<double>;
  FPBits xbits(x);

  uint64_t x_u = xbits.uintval();
  uint64_t x_abs = xbits.abs().uintval();

  // |x| <= 2^-53
  if (x_abs <= 0x3ca0'0000'0000'0000ULL) {
    // exp(x) ~ 1 + x
    return 1 + x;
  }

  // x <= log(2^-1075) || x >= 0x1.6232bdd7abcd3p+9 or inf/nan.

  // x <= log(2^-1075) or -inf/nan
  if (x_u >= 0xc087'4910'd52d'3052ULL) {
    // exp(-Inf) = 0
    if (xbits.is_inf())
      return 0.0;

    // exp(nan) = nan
    if (xbits.is_nan())
      return x;

    if (fputil::quick_get_round() == FE_UPWARD)
      return FPBits::min_subnormal().get_val();
    fputil::set_errno_if_required(ERANGE);
    fputil::raise_except_if_required(FE_UNDERFLOW);
    return 0.0;
  }

  // x >= round(log(MAX_NORMAL), D, RU) = 0x1.62e42fefa39fp+9 or +inf/nan
  // x is finite
  if (x_u < 0x7ff0'0000'0000'0000ULL) {
    int rounding = fputil::quick_get_round();
    if (rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO)
      return FPBits::max_normal().get_val();

    fputil::set_errno_if_required(ERANGE);
    fputil::raise_except_if_required(FE_OVERFLOW);
  }
  // x is +inf or nan
  return x + FPBits::inf().get_val();
}

} // namespace exp_internal

LIBC_INLINE double exp(double x) {
  using namespace exp_internal;
  using FPBits = typename fputil::FPBits<double>;
  FPBits xbits(x);

  uint64_t x_u = xbits.uintval();

  // Upper bound: max normal number = 2^1023 * (2 - 2^-52)
  // > round(log (2^1023 ( 2 - 2^-52 )), D, RU) = 0x1.62e42fefa39fp+9
  // > round(log (2^1023 ( 2 - 2^-52 )), D, RD) = 0x1.62e42fefa39efp+9
  // > round(log (2^1023 ( 2 - 2^-52 )), D, RN) = 0x1.62e42fefa39efp+9
  // > round(exp(0x1.62e42fefa39fp+9), D, RN) = infty

  // Lower bound: min denormal number / 2 = 2^-1075
  // > round(log(2^-1075), D, RN) = -0x1.74910d52d3052p9

  // Another lower bound: min normal number = 2^-1022
  // > round(log(2^-1022), D, RN) = -0x1.6232bdd7abcd2p9

  // x < log(2^-1075) or x >= 0x1.6232bdd7abcd3p+9 or |x| < 2^-53.
  if (LIBC_UNLIKELY(x_u >= 0xc0874910d52d3052 ||
                    (x_u < 0xbca0000000000000 && x_u >= 0x40862e42fefa39f0) ||
                    x_u < 0x3ca0000000000000)) {
    return set_exceptional(x);
  }

  // Now log(2^-1075) <= x <= -2^-53 or 2^-53 <= x < log(2^1023 * (2 - 2^-52))

  // Range reduction:
  // Let x = log(2) * (hi + mid1 + mid2) + lo
  // in which:
  //   hi is an integer
  //   mid1 * 2^6 is an integer
  //   mid2 * 2^12 is an integer
  // then:
  //   exp(x) = 2^hi * 2^(mid1) * 2^(mid2) * exp(lo).
  // With this formula:
  //   - multiplying by 2^hi is exact and cheap, simply by adding the exponent
  //     field.
  //   - 2^(mid1) and 2^(mid2) are stored in 2 x 64-element tables.
  //   - exp(lo) ~ 1 + lo + a0 * lo^2 + ...
  //
  // They can be defined by:
  //   hi + mid1 + mid2 = 2^(-12) * round(2^12 * log_2(e) * x)
  // If we store L2E = round(log2(e), D, RN), then:
  //   log2(e) - L2E ~ 1.5 * 2^(-56)
  // So the errors when computing in double precision is:
  //   | x * 2^12 * log_2(e) - D(x * 2^12 * L2E) | <=
  //  <= | x * 2^12 * log_2(e) - x * 2^12 * L2E | +
  //     + | x * 2^12 * L2E - D(x * 2^12 * L2E) |
  //  <= 2^12 * ( |x| * 1.5 * 2^-56 + eps(x))  for RN
  //     2^12 * ( |x| * 1.5 * 2^-56 + 2*eps(x)) for other rounding modes.
  // So if:
  //   hi + mid1 + mid2 = 2^(-12) * round(x * 2^12 * L2E) is computed entirely
  // in double precision, the reduced argument:
  //   lo = x - log(2) * (hi + mid1 + mid2) is bounded by:
  //   |lo| <= 2^-13 + (|x| * 1.5 * 2^-56 + 2*eps(x))
  //         < 2^-13 + (1.5 * 2^9 * 1.5 * 2^-56 + 2*2^(9 - 52))
  //         < 2^-13 + 2^-41
  //

  // The following trick computes the round(x * L2E) more efficiently
  // than using the rounding instructions, with the tradeoff for less accuracy,
  // and hence a slightly larger range for the reduced argument `lo`.
  //
  // To be precise, since |x| < |log(2^-1075)| < 1.5 * 2^9,
  //   |x * 2^12 * L2E| < 1.5 * 2^9 * 1.5 < 2^23,
  // So we can fit the rounded result round(x * 2^12 * L2E) in int32_t.
  // Thus, the goal is to be able to use an additional addition and fixed width
  // shift to get an int32_t representing round(x * 2^12 * L2E).
  //
  // Assuming int32_t using 2-complement representation, since the mantissa part
  // of a double precision is unsigned with the leading bit hidden, if we add an
  // extra constant C = 2^e1 + 2^e2 with e1 > e2 >= 2^25 to the product, the
  // part that are < 2^e2 in resulted mantissa of (x*2^12*L2E + C) can be
  // considered as a proper 2-complement representations of x*2^12*L2E.
  //
  // One small problem with this approach is that the sum (x*2^12*L2E + C) in
  // double precision is rounded to the least significant bit of the dorminant
  // factor C.  In order to minimize the rounding errors from this addition, we
  // want to minimize e1.  Another constraint that we want is that after
  // shifting the mantissa so that the least significant bit of int32_t
  // corresponds to the unit bit of (x*2^12*L2E), the sign is correct without
  // any adjustment.  So combining these 2 requirements, we can choose
  //   C = 2^33 + 2^32, so that the sign bit corresponds to 2^31 bit, and hence
  // after right shifting the mantissa, the resulting int32_t has correct sign.
  // With this choice of C, the number of mantissa bits we need to shift to the
  // right is: 52 - 33 = 19.
  //
  // Moreover, since the integer right shifts are equivalent to rounding down,
  // we can add an extra 0.5 so that it will become round-to-nearest, tie-to-
  // +infinity.  So in particular, we can compute:
  //   hmm = x * 2^12 * L2E + C,
  // where C = 2^33 + 2^32 + 2^-1, then if
  //   k = int32_t(lower 51 bits of double(x * 2^12 * L2E + C) >> 19),
  // the reduced argument:
  //   lo = x - log(2) * 2^-12 * k is bounded by:
  //   |lo| <= 2^-13 + 2^-41 + 2^-12*2^-19
  //         = 2^-13 + 2^-31 + 2^-41.
  //
  // Finally, notice that k only uses the mantissa of x * 2^12 * L2E, so the
  // exponent 2^12 is not needed.  So we can simply define
  //   C = 2^(33 - 12) + 2^(32 - 12) + 2^(-13 - 12), and
  //   k = int32_t(lower 51 bits of double(x * L2E + C) >> 19).

  // Rounding errors <= 2^-31 + 2^-41.
  double tmp = fputil::multiply_add(x, LOG2_E, 0x1.8000'0000'4p21);
  int k = static_cast<int>(cpp::bit_cast<uint64_t>(tmp) >> 19);
  double kd = static_cast<double>(k);

  uint32_t idx1 = (k >> 6) & 0x3f;
  uint32_t idx2 = k & 0x3f;
  int hi = k >> 12;

  bool denorm = (hi <= -1022);

  DoubleDouble exp_mid1{EXP2_MID1[idx1].mid, EXP2_MID1[idx1].hi};
  DoubleDouble exp_mid2{EXP2_MID2[idx2].mid, EXP2_MID2[idx2].hi};

  DoubleDouble exp_mid = fputil::quick_mult(exp_mid1, exp_mid2);

  // |x - (hi + mid1 + mid2) * log(2) - dx| < 2^11 * eps(M_LOG_2_EXP2_M12.lo)
  //                                        = 2^11 * 2^-13 * 2^-52
  //                                        = 2^-54.
  // |dx| < 2^-13 + 2^-30.
  double lo_h = fputil::multiply_add(kd, MLOG_2_EXP2_M12_HI, x); // exact
  double dx = fputil::multiply_add(kd, MLOG_2_EXP2_M12_MID, lo_h);

  // We use the degree-4 Taylor polynomial to approximate exp(lo):
  //   exp(lo) ~ 1 + lo + lo^2 / 2 + lo^3 / 6 + lo^4 / 24 = 1 + lo * P(lo)
  // So that the errors are bounded by:
  //   |P(lo) - expm1(lo)/lo| < |lo|^4 / 64 < 2^(-13 * 4) / 64 = 2^-58
  // Let P_ be an evaluation of P where all intermediate computations are in
  // double precision.  Using either Horner's or Estrin's schemes, the evaluated
  // errors can be bounded by:
  //      |P_(dx) - P(dx)| < 2^-51
  //   => |dx * P_(dx) - expm1(lo) | < 1.5 * 2^-64
  //   => 2^(mid1 + mid2) * |dx * P_(dx) - expm1(lo)| < 1.5 * 2^-63.
  // Since we approximate
  //   2^(mid1 + mid2) ~ exp_mid.hi + exp_mid.lo,
  // We use the expression:
  //    (exp_mid.hi + exp_mid.lo) * (1 + dx * P_(dx)) ~
  //  ~ exp_mid.hi + (exp_mid.hi * dx * P_(dx) + exp_mid.lo)
  // with errors bounded by 1.5 * 2^-63.

  double mid_lo = dx * exp_mid.hi;

  // Approximate expm1(dx)/dx ~ 1 + dx / 2 + dx^2 / 6 + dx^3 / 24.
  double p = poly_approx_d(dx);

  double lo = fputil::multiply_add(p, mid_lo, exp_mid.lo);

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (LIBC_UNLIKELY(denorm)) {
    return ziv_test_denorm</*SKIP_ZIV_TEST=*/true>(hi, exp_mid.hi, lo,
                                                   EXP_ERR_D)
        .value();
  } else {
    // to multiply by 2^hi, a fast way is to simply add hi to the exponent
    // field.
    int64_t exp_hi = static_cast<int64_t>(hi) << FPBits::FRACTION_LEN;
    double r =
        cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(exp_mid.hi + lo));
    return r;
  }
#else
  if (LIBC_UNLIKELY(denorm)) {
    if (auto r = ziv_test_denorm(hi, exp_mid.hi, lo, EXP_ERR_D);
        LIBC_LIKELY(r.has_value()))
      return r.value();
  } else {
    double upper = exp_mid.hi + (lo + EXP_ERR_D);
    double lower = exp_mid.hi + (lo - EXP_ERR_D);

    if (LIBC_LIKELY(upper == lower)) {
      // to multiply by 2^hi, a fast way is to simply add hi to the exponent
      // field.
      int64_t exp_hi = static_cast<int64_t>(hi) << FPBits::FRACTION_LEN;
      double r = cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(upper));
      return r;
    }
  }

  // Use double-double
  DoubleDouble r_dd = exp_double_double(x, kd, exp_mid);

  if (LIBC_UNLIKELY(denorm)) {
    if (auto r = ziv_test_denorm(hi, r_dd.hi, r_dd.lo, EXP_ERR_DD);
        LIBC_LIKELY(r.has_value()))
      return r.value();
  } else {
    double upper_dd = r_dd.hi + (r_dd.lo + EXP_ERR_DD);
    double lower_dd = r_dd.hi + (r_dd.lo - EXP_ERR_DD);

    if (LIBC_LIKELY(upper_dd == lower_dd)) {
      int64_t exp_hi = static_cast<int64_t>(hi) << FPBits::FRACTION_LEN;
      double r =
          cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(upper_dd));
      return r;
    }
  }

  // Use 128-bit precision
  Float128 r_f128 = exp_f128(x, kd, idx1, idx2);

  return static_cast<double>(r_f128);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP_H
