//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// 128-bit accurate path for double-precision pow(x, y).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_POW_ACCURATE_128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_POW_ACCURATE_128_H

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/big_int.h"
#include "src/__support/common.h"
#include "src/__support/frac128.h"
#include "src/__support/frac64.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/exp2_accurate_128.h"
#include "src/__support/math/pow_accurate_256.h"
#include "src/__support/math/pow_utils.h"
#include "src/__support/sign.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace pow_internal {

using DFloat128 = typename fputil::DyadicFloat<128>;

// Accurate log2(x) in 128-bit precision using 2-step integer range reduction:
//   x = 2^x_e * m_x, with 1 <= m_x < 2
//   log2(x) = x_e + (-log2(r1)) + (-log2(r2)) + log2(1 + dx).
// log2(1 + dx) is evaluated with a degree-15 prefix of LOG2_POLY_256 using a
// 2-tier Horner scheme (Frac64 and Frac128).
LIBC_COLD LIBC_INLINE DFloat128 log2_f128(int x_e, uint64_t x_mant) {
  if (LIBC_UNLIKELY(x_mant == 0))
    return df_from_int<128>(x_e);

  unsigned idx1 = 0;
  int k2 = 0;
  int64_t dx_int = log2_range_reduce(x_mant, idx1, k2); // lsb = 2^-65
  bool dx_is_neg = (dx_int < 0);
  uint64_t abs_dx =
      static_cast<uint64_t>(dx_is_neg ? -dx_int : dx_int); // lsb = 2^-65

  Frac64 y64(abs_dx >> 1);                // lsb = 2^-64
  Frac128 y({abs_dx << 63, abs_dx >> 1}); // lsb = 2^-128

  // Step 1: 64-bit evaluation for degrees 8..15 (|dx|^8 < 2^-63.7):
  Frac64 p64 = LOG2_POLY_128[7].to_frac64(); // lsb = 2^-64
  LIBC_LOOP_NOUNROLL
  for (int k = 6; k >= 0; --k) {
    Frac64 py = p64 * y64;
    Frac64 c = LOG2_POLY_128[k].to_frac64();
    p64 = dx_is_neg ? (c + py) : (c - py); // lsb = 2^-64
  }

  // Step 2: 128-bit evaluation for degrees 0..7:
  Frac128 p({0, p64.val[0]}); // lsb = 2^-128
  LIBC_LOOP_NOUNROLL
  for (int k = 7; k >= 0; --k) {
    Frac128 py = p * y;
    Frac128 c = LOG2_POLY_256[k].to_frac128();
    p = dx_is_neg ? (c + py) : (c - py); // lsb = 2^-128
  }

  // Normalize abs_dx before multiplying by p so near-1.0 inputs (x_e - log2(r)
  // == 0) retain full 128-bit precision without shifting out low bits.
  int lz = cpp::countl_zero(abs_dx);
  uint64_t norm_dx = abs_dx << lz;                  // lsb = 2^-(65 + lz)
  Frac128 norm_log2_1p = Frac128({0, norm_dx}) * p; // lsb = 2^-(128 + lz)
  if (LIBC_UNLIKELY((x_e == 0 && idx1 == 0 && k2 == 0) ||
                    (x_e == -1 && idx1 == 16 && k2 == 0))) {
    return df_norm_1bit<128>(dx_is_neg ? Sign::NEG : Sign::POS, -128 - lz,
                             UInt<128>(norm_log2_1p.val));
  }

  Frac128 log2_1p = norm_log2_1p >> static_cast<size_t>(lz + 1); // lsb = 2^-127
  Frac128 log2_rd = LOG2_R1_FRAC256[idx1].to_frac128() +
                    LOG2_R2_FRAC256[k2 + 4].to_frac128(); // lsb = 2^-127
  Frac128 log2_m =
      dx_is_neg ? (log2_rd - log2_1p) : (log2_rd + log2_1p); // lsb = 2^-127

  return combine_log2_df<128>(x_e, UInt<128>(log2_m.val));
}

// Accurate pow(x, y) in 128-bit precision.
LIBC_COLD LIBC_NOINLINE LIBC_INLINE double
pow_accurate(double y, bool is_neg, int x_e, uint64_t x_mant) {
  if (LIBC_UNLIKELY(x_e == 0 && x_mant == 0))
    return is_neg ? -1.0 : 1.0;

  Sign out_sign = is_neg ? Sign::NEG : Sign::POS;
  using FPBits = fputil::FPBits<double>;
  FPBits ybits(y);
  int exp_y = ybits.get_exponent();
  uint64_t mant_y = (ybits.get_mantissa() | (1ULL << 52)) << 11;

  DFloat128 log2_x = log2_f128(x_e, x_mant);
  uint64_t z_hi = mul_uint64(log2_x.mantissa, mant_y);
  uint64_t z_lo = log2_x.mantissa.val[0];

  DFloat128 z;
  z.sign = (ybits.sign() == log2_x.sign) ? Sign::POS : Sign::NEG;
  z.exponent = log2_x.exponent + exp_y + 1;
  z.mantissa = UInt<128>({log2_x.mantissa.val[1], z_hi});

  // 2^1025 > max normal double; 2^-1076 < min subnormal double.
  int msb = static_cast<int>(z_hi >> 63);
  int z_exp = z.exponent + 126 + msb;
  if (LIBC_UNLIKELY(z_exp >= 10)) {
    uint64_t int_z = (z_exp > 10) ? 2048 : (z_hi >> (52 + msb));
    if (!z.sign.is_neg() && int_z >= 1025)
      return fputil::set_overflow<double>(out_sign);
    if (z.sign.is_neg() && int_z >= 1076)
      return fputil::set_underflow<double>(out_sign);
  }

  // For 0 < |z| < 2^-54, x^y is strictly in (1 - 2^-54, 1 + 2^-53) \ {1}.
  DFloat128 r;
  if (LIBC_UNLIKELY(z_exp <= -55)) {
    r.sign = out_sign;
    if (z.sign.is_neg()) {
      r.exponent = -128;
      r.mantissa = UInt<128>({~0ULL, ~0ULL});
    } else {
      r.exponent = -127;
      r.mantissa = UInt<128>({1ULL, 1ULL << 63});
    }
    return static_cast<double>(r);
  }

  r = double_eval::exp2_df128(z, z_lo);
  r.sign = out_sign;

  int unbiased_exp = r.exponent + 127;
  if (LIBC_UNLIKELY(unbiased_exp >= 1024))
    return fputil::set_overflow<double>(out_sign);

  // Check if r is close to a 54-bit rounding boundary (either an exact 53-bit
  // float or a midpoint).
  // Lauter & Lefevre (2009) showed that if x^y is not an exact 54-bit number,
  // |x^y - o_54(x^y)| / x^y >= 2^-114 (distance >= 2^13 in r.mantissa).
  // For normal outputs (unbiased_exp >= -1022), the 54 leading bits occupy bits
  // [127:74] of r.mantissa. Extracting frac_hi = bits [73:10] (1 unit = 2^10 in
  // r.mantissa) bounds the evaluation error 32 * |y| + 2^13 (< 2^(exp_y + 6) +
  // 2^13) by eps10 = 8 + 2^max(exp_y - 4, 0) in 2^10 units, reducing the
  // two-sided boundary check to a single unsigned wraparound compare.
  bool is_boundary_candidate = true;
  if (LIBC_LIKELY(unbiased_exp >= -1022)) {
    uint64_t frac_hi = (r.mantissa.val[1] << 54) | (r.mantissa.val[0] >> 10);
    int shift_e = (exp_y > 4) ? (exp_y - 4) : 0;
    uint64_t eps10 = 8ULL + (1ULL << shift_e);
    is_boundary_candidate = (frac_hi + eps10) < (eps10 << 1);
  }

  if (LIBC_UNLIKELY(is_boundary_candidate)) {
    uint64_t exact_m = 0;
    int exact_exp = 0;
    if (is_exact_rounding_boundary(x_e, x_mant, y, exact_m, exact_exp)) {
      int l = 64 - cpp::countl_zero(exact_m);
      r.exponent = exact_exp + l - 128;
      r.mantissa = UInt<128>({0, exact_m << (64 - l)});
    } else {
      r = pow_accurate_256(ybits.sign(), exp_y, mant_y, is_neg, x_e, x_mant);
    }
  }

  if (LIBC_UNLIKELY(r.exponent + 127 >= 1024))
    return fputil::set_overflow<double>(out_sign);
  double res = static_cast<double>(r);
  if (LIBC_UNLIKELY(FPBits(res).is_inf()))
    return fputil::set_overflow<double>(out_sign);
  return res;
}

} // namespace pow_internal
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_POW_ACCURATE_128_H
