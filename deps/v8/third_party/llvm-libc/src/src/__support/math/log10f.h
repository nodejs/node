//===-- Implementation header for log10f ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOG10F_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOG10F_H

#include "common_constants.h" // Lookup table for (1/f)
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FMA.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/macros/properties/cpu_features.h"

// This is an algorithm for log10(x) in single precision which is
// correctly rounded for all rounding modes, based on the implementation of
// log10(x) from the RLIBM project at:
// https://people.cs.rutgers.edu/~sn349/rlibm

// Step 1 - Range reduction:
//   For x = 2^m * 1.mant, log(x) = m * log10(2) + log10(1.m)
//   If x is denormal, we normalize it by multiplying x by 2^23 and subtracting
//   m by 23.

// Step 2 - Another range reduction:
//   To compute log(1.mant), let f be the highest 8 bits including the hidden
// bit, and d be the difference (1.mant - f), i.e. the remaining 16 bits of the
// mantissa. Then we have the following approximation formula:
//   log10(1.mant) = log10(f) + log10(1.mant / f)
//                 = log10(f) + log10(1 + d/f)
//                 ~ log10(f) + P(d/f)
// since d/f is sufficiently small.
//   log10(f) and 1/f are then stored in two 2^7 = 128 entries look-up tables.

// Step 3 - Polynomial approximation:
//   To compute P(d/f), we use a single degree-5 polynomial in double precision
// which provides correct rounding for all but few exception values.
//   For more detail about how this polynomial is obtained, please refer to the
// papers:
//   Lim, J. and Nagarakatte, S., "One Polynomial Approximation to Produce
// Correctly Rounded Results of an Elementary Function for Multiple
// Representations and Rounding Modes", Proceedings of the 49th ACM SIGPLAN
// Symposium on Principles of Programming Languages (POPL-2022), Philadelphia,
// USA, Jan. 16-22, 2022.
// https://people.cs.rutgers.edu/~sn349/papers/rlibmall-popl-2022.pdf
//   Aanjaneya, M., Lim, J., and Nagarakatte, S., "RLibm-Prog: Progressive
// Polynomial Approximations for Fast Correctly Rounded Math Libraries",
// Dept. of Comp. Sci., Rutgets U., Technical Report DCS-TR-758, Nov. 2021.
// https://arxiv.org/pdf/2111.12852.pdf.

namespace LIBC_NAMESPACE_DECL {

namespace math {

LIBC_INLINE float log10f(float x) {
  using namespace common_constants_internal;
  constexpr double LOG10_2 = 0x1.34413509f79ffp-2;
  // Lookup table for -log10(r) where r is defined in common_constants.cpp.
  constexpr double LOG10_R[128] = {
      0x0.0000000000000p+0, 0x1.be76bd77b4fc3p-9, 0x1.c03a80ae5e054p-8,
      0x1.51824c7587ebp-7,  0x1.c3d0837784c41p-7, 0x1.1b85d6044e9aep-6,
      0x1.559bd2406c3bap-6, 0x1.902c31d62a843p-6, 0x1.cb38fccd8bfdbp-6,
      0x1.e8eeb09f2f6cbp-6, 0x1.125d0432ea20ep-5, 0x1.30838cdc2fbfdp-5,
      0x1.3faf7c663060ep-5, 0x1.5e3966b7e9295p-5, 0x1.7d070145f4fd7p-5,
      0x1.8c878eeb05074p-5, 0x1.abbcebd84fcap-5,  0x1.bb7209d1e24e5p-5,
      0x1.db11ed766abf4p-5, 0x1.eafd05035bd3bp-5, 0x1.0585283764178p-4,
      0x1.0d966cc6500fap-4, 0x1.1dd5460c8b16fp-4, 0x1.2603072a25f82p-4,
      0x1.367ba3aaa1883p-4, 0x1.3ec6ad5407868p-4, 0x1.4f7aad9bbcbafp-4,
      0x1.57e3d47c3af7bp-4, 0x1.605735ee985f1p-4, 0x1.715d0ce367afcp-4,
      0x1.79efb57b0f803p-4, 0x1.828cfed29a215p-4, 0x1.93e7de0fc3e8p-4,
      0x1.9ca5aa1729f45p-4, 0x1.a56e8325f5c87p-4, 0x1.ae4285509950bp-4,
      0x1.b721cd17157e3p-4, 0x1.c902a19e65111p-4, 0x1.d204698cb42bdp-4,
      0x1.db11ed766abf4p-4, 0x1.e42b4c16caaf3p-4, 0x1.ed50a4a26eafcp-4,
      0x1.ffbfc2bbc7803p-4, 0x1.0484e4942aa43p-3, 0x1.093025a19976cp-3,
      0x1.0de1b56356b04p-3, 0x1.1299a4fb3e306p-3, 0x1.175805d1587c1p-3,
      0x1.1c1ce9955c0c6p-3, 0x1.20e8624038fedp-3, 0x1.25ba8215af7fcp-3,
      0x1.2a935ba5f1479p-3, 0x1.2f7301cf4e87bp-3, 0x1.345987bfeea91p-3,
      0x1.394700f7953fdp-3, 0x1.3e3b8149739d4p-3, 0x1.43371cde076c2p-3,
      0x1.4839e83506c87p-3, 0x1.4d43f8275a483p-3, 0x1.525561e9256eep-3,
      0x1.576e3b0bde0a7p-3, 0x1.5c8e998072fe2p-3, 0x1.61b6939983048p-3,
      0x1.66e6400da3f77p-3, 0x1.6c1db5f9bb336p-3, 0x1.6c1db5f9bb336p-3,
      0x1.715d0ce367afcp-3, 0x1.76a45cbb7e6ffp-3, 0x1.7bf3bde099f3p-3,
      0x1.814b4921bd52bp-3, 0x1.86ab17c10bc7fp-3, 0x1.86ab17c10bc7fp-3,
      0x1.8c13437695532p-3, 0x1.9183e673394fap-3, 0x1.96fd1b639fc09p-3,
      0x1.9c7efd734a2f9p-3, 0x1.a209a84fbcff8p-3, 0x1.a209a84fbcff8p-3,
      0x1.a79d382bc21d9p-3, 0x1.ad39c9c2c608p-3,  0x1.b2df7a5c50299p-3,
      0x1.b2df7a5c50299p-3, 0x1.b88e67cf9798p-3,  0x1.be46b087354bcp-3,
      0x1.c4087384f4f8p-3,  0x1.c4087384f4f8p-3,  0x1.c9d3d065c5b42p-3,
      0x1.cfa8e765cbb72p-3, 0x1.cfa8e765cbb72p-3, 0x1.d587d96494759p-3,
      0x1.db70c7e96e7f3p-3, 0x1.db70c7e96e7f3p-3, 0x1.e163d527e68cfp-3,
      0x1.e76124046b3f3p-3, 0x1.e76124046b3f3p-3, 0x1.ed68d819191fcp-3,
      0x1.f37b15bab08d1p-3, 0x1.f37b15bab08d1p-3, 0x1.f99801fdb749dp-3,
      0x1.ffbfc2bbc7803p-3, 0x1.ffbfc2bbc7803p-3, 0x1.02f93f4c87101p-2,
      0x1.06182e84fd4acp-2, 0x1.06182e84fd4acp-2, 0x1.093cc32c90f84p-2,
      0x1.093cc32c90f84p-2, 0x1.0c6711d6abd7ap-2, 0x1.0f972f87ff3d6p-2,
      0x1.0f972f87ff3d6p-2, 0x1.12cd31b9c99ffp-2, 0x1.12cd31b9c99ffp-2,
      0x1.16092e5d3a9a6p-2, 0x1.194b3bdef6b9ep-2, 0x1.194b3bdef6b9ep-2,
      0x1.1c93712abc7ffp-2, 0x1.1c93712abc7ffp-2, 0x1.1fe1e5af2c141p-2,
      0x1.1fe1e5af2c141p-2, 0x1.2336b161b3337p-2, 0x1.2336b161b3337p-2,
      0x1.2691ecc29f042p-2, 0x1.2691ecc29f042p-2, 0x1.29f3b0e15584bp-2,
      0x1.29f3b0e15584bp-2, 0x1.2d5c1760b86bbp-2, 0x1.2d5c1760b86bbp-2,
      0x1.30cb3a7bb3625p-2, 0x1.34413509f79ffp-2};

  using FPBits = typename fputil::FPBits<float>;

  FPBits xbits(x);
  uint32_t x_u = xbits.uintval();

  // Exact powers of 10 and other hard-to-round cases.
  if (LIBC_UNLIKELY((x_u & 0x3FF) == 0)) {
    switch (x_u) {
    case 0x3f80'0000U: // x = 1
      return 0.0f;
    case 0x4120'0000U: // x = 10
      return 1.0f;
    case 0x42c8'0000U: // x = 100
      return 2.0f;
    case 0x447a'0000U: // x = 1,000
      return 3.0f;
    case 0x461c'4000U: // x = 10,000
      return 4.0f;
    case 0x47c3'5000U: // x = 100,000
      return 5.0f;
    case 0x4974'2400U: // x = 1,000,000
      return 6.0f;
    }
  } else {
    switch (x_u) {
    case 0x4b18'9680U: // x = 10,000,000
      return 7.0f;
    case 0x4cbe'bc20U: // x = 100,000,000
      return 8.0f;
    case 0x4e6e'6b28U: // x = 1,000,000,000
      return 9.0f;
    case 0x5015'02f9U: // x = 10,000,000,000
      return 10.0f;
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    case 0x0efe'ee7aU: // x = 0x1.fddcf4p-98f
      return fputil::round_result_slightly_up(-0x1.d33a46p+4f);
    case 0x3f5f'de1bU: // x = 0x1.bfbc36p-1f
      return fputil::round_result_slightly_up(-0x1.dd2c6ep-5f);
    case 0x3f80'70d8U: // x = 0x1.00e1bp0f
      return fputil::round_result_slightly_up(0x1.8762c4p-10f);
#ifndef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
    case 0x08ae'a356U: // x = 0x1.5d46acp-110f
      return fputil::round_result_slightly_up(-0x1.07d3b4p+5f);
    case 0x120b'93dcU: // x = 0x1.1727b8p-91f
      return fputil::round_result_slightly_down(-0x1.b5b2aep+4f);
    case 0x13ae'78d3U: // x = 0x1.5cf1a6p-88f
      return fputil::round_result_slightly_down(-0x1.a5b2aep+4f);
    case 0x4f13'4f83U: // x = 2471461632.0
      return fputil::round_result_slightly_down(0x1.2c9314p+3f);
    case 0x7956'ba5eU: // x = 69683218960000541503257137270226944.0
      return fputil::round_result_slightly_up(0x1.16bebap+5f);
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    }
  }

  int m = -FPBits::EXP_BIAS;

  if (LIBC_UNLIKELY(x_u < FPBits::min_normal().uintval() ||
                    x_u > FPBits::max_normal().uintval())) {
    if (x == 0.0f) {
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
    if (xbits.is_inf_or_nan()) {
      return x;
    }
    // Normalize denormal inputs.
    xbits = FPBits(xbits.get_val() * 0x1.0p23f);
    m -= 23;
    x_u = xbits.uintval();
  }

  // Add unbiased exponent.
  m += static_cast<int>(x_u >> 23);
  // Extract 7 leading fractional bits of the mantissa
  int index = (x_u >> 16) & 0x7F;
  // Set bits to 1.m
  xbits.set_biased_exponent(0x7F);

  float u = xbits.get_val();
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
  double v =
      static_cast<double>(fputil::multiply_add(u, R[index], -1.0f)); // Exact.
#else
  double v = fputil::multiply_add(static_cast<double>(u),
                                  static_cast<double>(R[index]), -1.0); // Exact
#endif // LIBC_TARGET_CPU_HAS_FMA_FLOAT

  // Degree-5 polynomial approximation of log10 generated by:
  // > P = fpminimax(log10(1 + x)/x, 4, [|D...|], [-2^-8, 2^-7]);
  constexpr double COEFFS[5] = {0x1.bcb7b1526e2e5p-2, -0x1.bcb7b1528d43dp-3,
                                0x1.287a77eb4ca0dp-3, -0x1.bcb8110a181b5p-4,
                                0x1.60e7e3e747129p-4};
  double v2 = v * v; // Exact
  double p2 = fputil::multiply_add(v, COEFFS[4], COEFFS[3]);
  double p1 = fputil::multiply_add(v, COEFFS[2], COEFFS[1]);
  double p0 = fputil::multiply_add(v, COEFFS[0], LOG10_R[index]);
  double r = fputil::multiply_add(static_cast<double>(m), LOG10_2,
                                  fputil::polyeval(v2, p0, p1, p2));

  return static_cast<float>(r);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOG10F_H
