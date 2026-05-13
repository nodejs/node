//===-- Implementation header for expm1 -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXPM1_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXPM1_H

#include "common_constants.h" // Lookup tables EXP_M1 and EXP_M2.
#include "exp_constants.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/FPUtil/triple_double.h"
#include "src/__support/common.h"
#include "src/__support/integer_literals.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace expm1_internal {

#if ((LIBC_MATH & LIBC_MATH_SKIP_ACCURATE_PASS) != 0)
#define LIBC_MATH_EXPM1_SKIP_ACCURATE_PASS
#endif

using fputil::DoubleDouble;
using fputil::TripleDouble;
using Float128 = typename fputil::DyadicFloat<128>;

using LIBC_NAMESPACE::operator""_u128;

// log2(e)
LIBC_INLINE_VAR constexpr double LOG2_E = 0x1.71547652b82fep+0;

// Error bounds:
// Errors when using double precision.
// 0x1.8p-63;
LIBC_INLINE_VAR constexpr uint64_t ERR_D = 0x3c08000000000000;
// Errors when using double-double precision.
// 0x1.0p-99
[[maybe_unused]] LIBC_INLINE_VAR constexpr uint64_t ERR_DD = 0x39c0000000000000;

// -2^-12 * log(2)
// > a = -2^-12 * log(2);
// > b = round(a, 30, RN);
// > c = round(a - b, 30, RN);
// > d = round(a - b - c, D, RN);
// Errors < 1.5 * 2^-133
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_HI = -0x1.62e42ffp-13;
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_MID = 0x1.718432a1b0e26p-47;
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_MID_30 = 0x1.718432ap-47;
LIBC_INLINE_VAR constexpr double MLOG_2_EXP2_M12_LO = 0x1.b0e2633fe0685p-79;

using namespace common_constants_internal;

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

// Polynomial approximation with double-double precision:
// Return expm1(dx) / dx ~ 1 + dx / 2 + dx^2 / 6 + ... + dx^6 / 5040
// For |dx| < 2^-13 + 2^-30:
//   | output - expm1(dx) | < 2^-101
LIBC_INLINE constexpr DoubleDouble poly_approx_dd(const DoubleDouble &dx) {
  // Taylor polynomial.
  constexpr DoubleDouble COEFFS[] = {
      {0, 0x1p0},                                      // 1
      {0, 0x1p-1},                                     // 1/2
      {0x1.5555555555555p-57, 0x1.5555555555555p-3},   // 1/6
      {0x1.5555555555555p-59, 0x1.5555555555555p-5},   // 1/24
      {0x1.1111111111111p-63, 0x1.1111111111111p-7},   // 1/120
      {-0x1.f49f49f49f49fp-65, 0x1.6c16c16c16c17p-10}, // 1/720
      {0x1.a01a01a01a01ap-73, 0x1.a01a01a01a01ap-13},  // 1/5040
  };

  DoubleDouble p = fputil::polyeval(dx, COEFFS[0], COEFFS[1], COEFFS[2],
                                    COEFFS[3], COEFFS[4], COEFFS[5], COEFFS[6]);
  return p;
}

// Polynomial approximation with 128-bit precision:
// Return (exp(dx) - 1)/dx ~ 1 + dx / 2 + dx^2 / 6 + ... + dx^6 / 5040
// For |dx| < 2^-13 + 2^-30:
//   | output - exp(dx) | < 2^-126.
[[maybe_unused]] LIBC_INLINE constexpr Float128
poly_approx_f128(const Float128 &dx) {
  constexpr Float128 COEFFS_128[]{
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
                                COEFFS_128[6]);
  return p;
}

#ifdef DEBUGDEBUG
std::ostream &operator<<(std::ostream &OS, const Float128 &r) {
  OS << (r.sign == Sign::NEG ? "-(" : "(") << r.mantissa.val[0] << " + "
     << r.mantissa.val[1] << " * 2^64) * 2^" << r.exponent << "\n";
  return OS;
}

std::ostream &operator<<(std::ostream &OS, const DoubleDouble &r) {
  OS << std::hexfloat << "(" << r.hi << " + " << r.lo << ")"
     << std::defaultfloat << "\n";
  return OS;
}
#endif

// Compute exp(x) - 1 using 128-bit precision.
// TODO(lntue): investigate triple-double precision implementation for this
// step.
[[maybe_unused]] LIBC_INLINE Float128 expm1_f128(double x, double kd, int idx1,
                                                 int idx2) {
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

  int hi = static_cast<int>(kd) >> 12;
  Float128 minus_one{Sign::NEG, -127 - hi,
                     0x80000000'00000000'00000000'00000000_u128};

  Float128 exp_mid_m1 = fputil::quick_add(exp_mid, minus_one);

  Float128 p = poly_approx_f128(dx);

  // r = exp_mid * (1 + dx * P) - 1
  //   = (exp_mid - 1) + (dx * exp_mid) * P
  Float128 r =
      fputil::multiply_add(fputil::quick_mul(exp_mid, dx), p, exp_mid_m1);

  r.exponent += hi;

#ifdef DEBUGDEBUG
  std::cout << "=== VERY SLOW PASS ===\n"
            << "        kd: " << kd << "\n"
            << "        hi: " << hi << "\n"
            << " minus_one: " << minus_one << "        dx: " << dx
            << "exp_mid_m1: " << exp_mid_m1 << "   exp_mid: " << exp_mid
            << "         p: " << p << "         r: " << r << std::endl;
#endif

  return r;
}

// Compute exp(x) - 1 with double-double precision.
LIBC_INLINE DoubleDouble exp_double_double(double x, double kd,
                                           const DoubleDouble &exp_mid,
                                           const DoubleDouble &hi_part) {
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
  DoubleDouble r =
      fputil::multiply_add(fputil::quick_mult(exp_mid, dx), p, hi_part);

#ifdef DEBUGDEBUG
  std::cout << "=== SLOW PASS ===\n"
            << "   dx: " << dx << "    p: " << p << "    r: " << r << std::endl;
#endif

  return r;
}

// Check for exceptional cases when
// |x| <= 2^-53 or x < log(2^-54) or x >= 0x1.6232bdd7abcd3p+9
LIBC_INLINE constexpr double set_exceptional(double x) {
  using FPBits = typename fputil::FPBits<double>;
  FPBits xbits(x);

  uint64_t x_u = xbits.uintval();
  uint64_t x_abs = xbits.abs().uintval();

  // |x| <= 2^-53.
  if (x_abs <= 0x3ca0'0000'0000'0000ULL) {
    // expm1(x) ~ x.

    if (LIBC_UNLIKELY(x_abs <= 0x0370'0000'0000'0000ULL)) {
      if (LIBC_UNLIKELY(x_abs == 0))
        return x;
      // |x| <= 2^-968, need to scale up a bit before rounding, then scale it
      // back down.
      return 0x1.0p-200 * fputil::multiply_add(x, 0x1.0p+200, 0x1.0p-1022);
    }

    // 2^-968 < |x| <= 2^-53.
    return fputil::round_result_slightly_up(x);
  }

  // x < log(2^-54) || x >= 0x1.6232bdd7abcd3p+9 or inf/nan.

  // x < log(2^-54) or -inf/nan
  if (x_u >= 0xc042'b708'8723'20e2ULL) {
    // expm1(-Inf) = -1
    if (xbits.is_inf())
      return -1.0;

    // exp(nan) = nan
    if (xbits.is_nan())
      return x;

    return fputil::round_result_slightly_up(-1.0);
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

} // namespace expm1_internal

LIBC_INLINE constexpr double expm1(double x) {
  using namespace expm1_internal;

  using FPBits = typename fputil::FPBits<double>;

  FPBits xbits(x);

  bool x_is_neg = xbits.is_neg();
  uint64_t x_u = xbits.uintval();

  // Upper bound: max normal number = 2^1023 * (2 - 2^-52)
  // > round(log (2^1023 ( 2 - 2^-52 )), D, RU) = 0x1.62e42fefa39fp+9
  // > round(log (2^1023 ( 2 - 2^-52 )), D, RD) = 0x1.62e42fefa39efp+9
  // > round(log (2^1023 ( 2 - 2^-52 )), D, RN) = 0x1.62e42fefa39efp+9
  // > round(exp(0x1.62e42fefa39fp+9), D, RN) = infty

  // Lower bound: log(2^-54) = -0x1.2b708872320e2p5
  // > round(log(2^-54), D, RN) = -0x1.2b708872320e2p5

  // x < log(2^-54) or x >= 0x1.6232bdd7abcd3p+9 or |x| <= 2^-53.

  if (LIBC_UNLIKELY(x_u >= 0xc042b708872320e2 ||
                    (x_u <= 0xbca0000000000000 && x_u >= 0x40862e42fefa39f0) ||
                    x_u <= 0x3ca0000000000000)) {
    return set_exceptional(x);
  }

  // Now log(2^-54) <= x <= -2^-53 or 2^-53 <= x < log(2^1023 * (2 - 2^-52))

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

  DoubleDouble exp_mid1{EXP2_MID1[idx1].mid, EXP2_MID1[idx1].hi};
  DoubleDouble exp_mid2{EXP2_MID2[idx2].mid, EXP2_MID2[idx2].hi};

  DoubleDouble exp_mid = fputil::quick_mult(exp_mid1, exp_mid2);

  // -2^(-hi)
  double one_scaled =
      FPBits::create_value(Sign::NEG, FPBits::EXP_BIAS - hi, 0).get_val();

  // 2^(mid1 + mid2) - 2^(-hi)
  DoubleDouble hi_part = x_is_neg ? fputil::exact_add(one_scaled, exp_mid.hi)
                                  : fputil::exact_add(exp_mid.hi, one_scaled);

  hi_part.lo += exp_mid.lo;

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

  // Finally, we have the following approximation formula:
  //   expm1(x) = 2^hi * 2^(mid1 + mid2) * exp(lo) - 1
  //            = 2^hi * ( 2^(mid1 + mid2) * exp(lo) - 2^(-hi) )
  //            ~ 2^hi * ( (exp_mid.hi - 2^-hi) +
  //                       + (exp_mid.hi * dx * P_(dx) + exp_mid.lo))

  double mid_lo = dx * exp_mid.hi;

  // Approximate expm1(dx)/dx ~ 1 + dx / 2 + dx^2 / 6 + dx^3 / 24.
  double p = poly_approx_d(dx);

  double lo = fputil::multiply_add(p, mid_lo, hi_part.lo);

  // TODO: The following line leaks encoding abstraction. Use FPBits methods
  // instead.
  uint64_t err = x_is_neg ? (static_cast<uint64_t>(-hi) << 52) : 0;

  double err_d = cpp::bit_cast<double>(ERR_D + err);

  double upper = hi_part.hi + (lo + err_d);
  double lower = hi_part.hi + (lo - err_d);

#ifdef DEBUGDEBUG
  std::cout << "=== FAST PASS ===\n"
            << "      x: " << std::hexfloat << x << std::defaultfloat << "\n"
            << "      k: " << k << "\n"
            << "   idx1: " << idx1 << "\n"
            << "   idx2: " << idx2 << "\n"
            << "     hi: " << hi << "\n"
            << "     dx: " << std::hexfloat << dx << std::defaultfloat << "\n"
            << "exp_mid: " << exp_mid << "hi_part: " << hi_part
            << " mid_lo: " << std::hexfloat << mid_lo << std::defaultfloat
            << "\n"
            << "      p: " << std::hexfloat << p << std::defaultfloat << "\n"
            << "     lo: " << std::hexfloat << lo << std::defaultfloat << "\n"
            << "  upper: " << std::hexfloat << upper << std::defaultfloat
            << "\n"
            << "  lower: " << std::hexfloat << lower << std::defaultfloat
            << "\n"
            << std::endl;
#endif

  if (LIBC_LIKELY(upper == lower)) {
    // to multiply by 2^hi, a fast way is to simply add hi to the exponent
    // field.
    int64_t exp_hi = static_cast<int64_t>(hi) << FPBits::FRACTION_LEN;
    double r = cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(upper));
    return r;
  }

  // Use double-double
  DoubleDouble r_dd = exp_double_double(x, kd, exp_mid, hi_part);

#ifdef LIBC_MATH_EXPM1_SKIP_ACCURATE_PASS
  int64_t exp_hi = static_cast<int64_t>(hi) << FPBits::FRACTION_LEN;
  double r =
      cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(r_dd.hi + r_dd.lo));
  return r;
#else
  double err_dd = cpp::bit_cast<double>(ERR_DD + err);

  double upper_dd = r_dd.hi + (r_dd.lo + err_dd);
  double lower_dd = r_dd.hi + (r_dd.lo - err_dd);

  if (LIBC_LIKELY(upper_dd == lower_dd)) {
    int64_t exp_hi = static_cast<int64_t>(hi) << FPBits::FRACTION_LEN;
    double r = cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(upper_dd));
    return r;
  }

  // Use 128-bit precision
  Float128 r_f128 = expm1_f128(x, kd, idx1, idx2);

  return static_cast<double>(r_f128);
#endif // LIBC_MATH_EXPM1_SKIP_ACCURATE_PASS
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXPM1_H
