//===-- Implementation header of sqrtf128 ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_SQRTF128_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_SQRTF128_H

#include "include/llvm-libc-types/float128.h"

#ifdef LIBC_TYPES_HAS_FLOAT128

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/common.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/uint128.h"

// Compute sqrtf128 with correct rounding for all rounding modes using integer
// arithmetic by Alexei Sibidanov (sibid@uvic.ca):
//   https://github.com/sibidanov/llvm-project/tree/as_sqrt_v2
//   https://github.com/sibidanov/llvm-project/tree/as_sqrt_v3
// TODO: Update the reference once Alexei's implementation is in the CORE-MATH
// project. https://github.com/llvm/llvm-project/issues/126794

// Let the input be expressed as x = 2^e * m_x,
// - Step 1: Range reduction
//   Let x_reduced = 2^(e % 2) * m_x,
//   Then sqrt(x) = 2^(e / 2) * sqrt(x_reduced), with
//     1 <= x_reduced < 4.
// - Step 2: Polynomial approximation
//   Approximate 1/sqrt(x_reduced) using polynomial approximation with the
//   result errors bounded by:
//     |r0 - 1/sqrt(x_reduced)| < 2^-32.
//   The computations are done in uint64_t.
// - Step 3: First Newton iteration
//   Let the scaled error defined by:
//     h0 = r0^2 * x_reduced - 1.
//   Then we compute the first Newton iteration:
//     r1 = r0 - r0 * h0 / 2.
//   The result is then bounded by:
//     |r1 - 1 / sqrt(x_reduced)| < 2^-62.
// - Step 4: Second Newton iteration
//   We calculate the scaled error from Step 3:
//     h1 = r1^2 * x_reduced - 1.
//   Then the second Newton iteration is computed by:
//     r2 = x_reduced * (r1 - r1 * h0 / 2)
//        ~ x_reduced * (1/sqrt(x_reduced)) = sqrt(x_reduced)
// - Step 5: Perform rounding test and correction if needed.
//     Rounding correction is done by computing the exact rounding errors:
//       x_reduced - r2^2.

namespace LIBC_NAMESPACE_DECL {
namespace math {

namespace sqrtf128_internal {

template <typename T, typename U = T> LIBC_INLINE constexpr T prod_hi(T, U);

// Get high part of integer multiplications.
// Use template to prevent implicit conversion.
template <>
LIBC_INLINE constexpr uint64_t prod_hi<uint64_t>(uint64_t x, uint64_t y) {
  return static_cast<uint64_t>(
      (static_cast<UInt128>(x) * static_cast<UInt128>(y)) >> 64);
}

// Get high part of unsigned 128x64 bit multiplication.
template <>
LIBC_INLINE constexpr UInt128 prod_hi<UInt128, uint64_t>(UInt128 x,
                                                         uint64_t y) {
  uint64_t x_lo = static_cast<uint64_t>(x);
  uint64_t x_hi = static_cast<uint64_t>(x >> 64);
  UInt128 xyl = static_cast<UInt128>(x_lo) * static_cast<UInt128>(y);
  UInt128 xyh = static_cast<UInt128>(x_hi) * static_cast<UInt128>(y);
  return xyh + (xyl >> 64);
}

// Get high part of signed 64x64 bit multiplication.
template <>
LIBC_INLINE constexpr int64_t prod_hi<int64_t>(int64_t x, int64_t y) {
  return static_cast<int64_t>(
      (static_cast<Int128>(x) * static_cast<Int128>(y)) >> 64);
}

// Get high 128-bit part of unsigned 128x128 bit multiplication.
template <>
LIBC_INLINE constexpr UInt128 prod_hi<UInt128>(UInt128 x, UInt128 y) {
  uint64_t x_lo = static_cast<uint64_t>(x);
  uint64_t x_hi = static_cast<uint64_t>(x >> 64);
  uint64_t y_lo = static_cast<uint64_t>(y);
  uint64_t y_hi = static_cast<uint64_t>(y >> 64);

  UInt128 xh_yh = static_cast<UInt128>(x_hi) * static_cast<UInt128>(y_hi);
  UInt128 xh_yl = static_cast<UInt128>(x_hi) * static_cast<UInt128>(y_lo);
  UInt128 xl_yh = static_cast<UInt128>(x_lo) * static_cast<UInt128>(y_hi);

  xh_yh += xh_yl >> 64;

  return xh_yh + (xl_yh >> 64);
}

// Get high 128-bit part of mixed sign 128x128 bit multiplication.
template <>
LIBC_INLINE constexpr Int128 prod_hi<Int128, UInt128>(Int128 x, UInt128 y) {
  UInt128 mask = static_cast<UInt128>(x >> 127);
  UInt128 negative_part = y & mask;
  UInt128 prod = prod_hi(static_cast<UInt128>(x), y);
  return static_cast<Int128>(prod - negative_part);
}

// Newton-Raphson first order step to improve accuracy of the result.
// For the initial approximation r0 ~ 1/sqrt(x), let
//   h = r0^2 * x - 1
// be its scaled error.  Then the first-order Newton-Raphson iteration is:
//   r1 = r0 - r0 * h / 2
// which has error bounded by:
//   |r1 - 1/sqrt(x)| < h^2 / 2.
LIBC_INLINE constexpr uint64_t rsqrt_newton_raphson(uint64_t m, uint64_t r) {
  uint64_t r2 = prod_hi(r, r);
  // h = r0^2*x - 1.
  int64_t h = static_cast<int64_t>(prod_hi(m, r2) + r2);
  // hr = r * h / 2
  int64_t hr = prod_hi(h, static_cast<int64_t>(r >> 1));
  return r - hr;
}

#ifdef LIBC_MATH_HAS_SMALL_TABLES
// Degree-12 minimax polynomials for 1/sqrt(x) on [1, 2].
LIBC_INLINE_VAR constexpr uint32_t RSQRT_COEFFS[12] = {
    0xb5947a4a, 0x2d651e32, 0x9ad50532, 0x2d28d093, 0x0d8be653, 0x04239014,
    0x01492449, 0x0066ff7d, 0x001e74a1, 0x000984cc, 0x00049abc, 0x00018340,
};

LIBC_INLINE constexpr uint64_t rsqrt_approx(uint64_t m) {
  int64_t x = static_cast<uint64_t>(m) ^ (uint64_t(1) << 63);
  int64_t x_26 = x >> 2;
  int64_t z = x >> 31;

  if (LIBC_UNLIKELY(z <= -4294967296))
    return ~(m >> 1);

  uint64_t x2 = static_cast<uint64_t>(z) * static_cast<uint64_t>(z);
  uint64_t x2_26 = x2 >> 5;
  x2 >>= 32;
  // Calculate the odd part of the polynomial using Horner's method.
  uint64_t c0 = RSQRT_COEFFS[8] + ((x2 * RSQRT_COEFFS[10]) >> 32);
  uint64_t c1 = RSQRT_COEFFS[6] + ((x2 * c0) >> 32);
  uint64_t c2 = RSQRT_COEFFS[4] + ((x2 * c1) >> 32);
  uint64_t c3 = RSQRT_COEFFS[2] + ((x2 * c2) >> 32);
  uint64_t c4 = RSQRT_COEFFS[0] + ((x2 * c3) >> 32);
  uint64_t odd =
      static_cast<uint64_t>((x >> 34) * static_cast<int64_t>(c4 >> 3)) + x_26;
  // Calculate the even part of the polynomial using Horner's method.
  uint64_t d0 = RSQRT_COEFFS[9] + ((x2 * RSQRT_COEFFS[11]) >> 32);
  uint64_t d1 = RSQRT_COEFFS[7] + ((x2 * d0) >> 32);
  uint64_t d2 = RSQRT_COEFFS[5] + ((x2 * d1) >> 32);
  uint64_t d3 = RSQRT_COEFFS[3] + ((x2 * d2) >> 32);
  uint64_t d4 = RSQRT_COEFFS[1] + ((x2 * d3) >> 32);
  uint64_t even = 0xd105eb806655d608ul + ((x2 * d4) >> 6) + x2_26;

  uint64_t r = even - odd; // error < 1.5e-10
  // Newton-Raphson first order step to improve accuracy of the result to almost
  // 64 bits.
  return rsqrt_newton_raphson(m, r);
}

#else
// Cubic minimax polynomials for 1/sqrt(x) on [1 + k/64, 1 + (k + 1)/64]
// for k = 0..63.
LIBC_INLINE_VAR constexpr uint32_t RSQRT_COEFFS[64][4] = {
    {0xffffffff, 0xfffff780, 0xbff55815, 0x9bb5b6e7},
    {0xfc0bd889, 0xfa1d6e7d, 0xb8a95a89, 0x938bf8f0},
    {0xf82ec882, 0xf473bea9, 0xb1bf4705, 0x8bed0079},
    {0xf467f280, 0xeefff2a1, 0xab309d4a, 0x84cdb431},
    {0xf0b6848c, 0xe9bf46f4, 0xa4f76232, 0x7e24037b},
    {0xed19b75e, 0xe4af2628, 0x9f0e1340, 0x77e6ca62},
    {0xe990cdad, 0xdfcd2521, 0x996f9b96, 0x720db8df},
    {0xe61b138e, 0xdb16ffde, 0x94174a00, 0x6c913cff},
    {0xe2b7dddf, 0xd68a967b, 0x8f00c812, 0x676a6f92},
    {0xdf6689b7, 0xd225ea80, 0x8a281226, 0x62930308},
    {0xdc267bea, 0xcde71c63, 0x8589702c, 0x5e05343e},
    {0xd8f7208e, 0xc9cc6948, 0x81216f2e, 0x59bbbcf8},
    {0xd5d7ea91, 0xc5d428ee, 0x7cecdb76, 0x55b1c7d6},
    {0xd2c8534e, 0xc1fccbc9, 0x78e8bb45, 0x51e2e592},
    {0xcfc7da32, 0xbe44d94a, 0x75124a0a, 0x4e4b0369},
    {0xccd6045f, 0xbaaaee41, 0x7166f40f, 0x4ae66284},
    {0xc9f25c5c, 0xb72dbb69, 0x6de45288, 0x47b19045},
    {0xc71c71c7, 0xb3cc040f, 0x6a882804, 0x44a95f5f},
    {0xc453d90f, 0xb0849cd4, 0x67505d2a, 0x41cae1a0},
    {0xc1982b2e, 0xad566a85, 0x643afdc8, 0x3f13625c},
    {0xbee9056f, 0xaa406113, 0x6146361f, 0x3c806169},
    {0xbc46092e, 0xa7418293, 0x5e70506d, 0x3a0f8e8e},
    {0xb9aedba5, 0xa458de58, 0x5bb7b2b1, 0x37bec572},
    {0xb72325b7, 0xa1859022, 0x591adc9a, 0x358c09e2},
    {0xb4a293c2, 0x9ec6bf52, 0x569865a7, 0x33758476},
    {0xb22cd56d, 0x9c1b9e36, 0x542efb6a, 0x31797f8a},
    {0xafc19d86, 0x9983695c, 0x51dd5ffb, 0x2f96647a},
    {0xad60a1d1, 0x96fd66f7, 0x4fa2687c, 0x2dcab91f},
    {0xab099ae9, 0x9488e64b, 0x4d7cfbc9, 0x2c151d8a},
    {0xa8bc441a, 0x92253f20, 0x4b6c1139, 0x2a7449ef},
    {0xa6785b42, 0x8fd1d14a, 0x496eaf82, 0x28e70cc3},
    {0xa43da0ae, 0x8d8e042a, 0x4783eba7, 0x276c4900},
    {0xa20bd701, 0x8b594648, 0x45aae80a, 0x2602f493},
    {0x9fe2c315, 0x89330ce4, 0x43e2d382, 0x24aa16ec},
    {0x9dc22be4, 0x871ad399, 0x422ae88c, 0x2360c7af},
    {0x9ba9da6c, 0x85101c05, 0x40826c88, 0x22262d7b},
    {0x99999999, 0x83126d70, 0x3ee8af07, 0x20f97cd2},
    {0x97913630, 0x81215480, 0x3d5d0922, 0x1fd9f714},
    {0x95907eb8, 0x7f3c62ef, 0x3bdedce0, 0x1ec6e994},
    {0x93974369, 0x7d632f45, 0x3a6d94a9, 0x1dbfacbb},
    {0x91a55615, 0x7b955498, 0x3908a2be, 0x1cc3a33b},
    {0x8fba8a1c, 0x79d2724e, 0x37af80bf, 0x1bd23960},
    {0x8dd6b456, 0x781a2be4, 0x3661af39, 0x1aeae458},
    {0x8bf9ab07, 0x766c28ba, 0x351eb539, 0x1a0d21a2},
    {0x8a2345cc, 0x74c813dd, 0x33e61feb, 0x19387676},
    {0x88535d90, 0x732d9bdc, 0x32b7823a, 0x186c6f3e},
    {0x8689cc7e, 0x719c7297, 0x3192747d, 0x17a89f21},
    {0x84c66df1, 0x70144d19, 0x30769424, 0x16ec9f89},
    {0x83091e6a, 0x6e94e36c, 0x2f63836f, 0x16380fbf},
    {0x8151bb87, 0x6d1df079, 0x2e58e925, 0x158a9484},
    {0x7fa023f1, 0x6baf31de, 0x2d567053, 0x14e3d7ba},
    {0x7df43758, 0x6a4867d3, 0x2c5bc811, 0x1443880e},
    {0x7c4dd664, 0x68e95508, 0x2b68a346, 0x13a958ab},
    {0x7aace2b0, 0x6791be86, 0x2a7cb871, 0x131500ee},
    {0x79113ebc, 0x66416b95, 0x2997c17a, 0x12863c29},
    {0x777acde8, 0x64f825a1, 0x28b97b82, 0x11fcc95c},
    {0x75e9746a, 0x63b5b822, 0x27e1a6b4, 0x11786b03},
    {0x745d1746, 0x6279f081, 0x2710061d, 0x10f8e6da},
    {0x72d59c46, 0x61449e06, 0x26445f86, 0x107e05ac},
    {0x7152e9f4, 0x601591be, 0x257e7b4d, 0x10079327},
    {0x6fd4e793, 0x5eec9e6b, 0x24be2445, 0x0f955da9},
    {0x6e5b7d16, 0x5dc9986e, 0x24032795, 0x0f273620},
    {0x6ce6931d, 0x5cac55b7, 0x234d5496, 0x0ebcefdb},
    {0x6b7612ec, 0x5b94adb2, 0x229c7cbc, 0x0e56606e},
};

// Approximate rsqrt with cubic polynomials.
// The range [1,2] is splitted into 64 equal sub-ranges and the reciprocal
// square root is approximated by a cubic polynomial by the minimax method in
// each subrange. The approximation accuracy fits into 32-33 bits and thus it is
// natural to round coefficients into 32 bit. The constant coefficient can be
// rounded to 33 bits since the most significant bit is always 1 and implicitly
// assumed in the table.
LIBC_INLINE constexpr uint64_t rsqrt_approx(uint64_t m) {
  // ULP(m) = 2^-64.
  // Use the top 6 bits as index for looking up polynomial coeffs.
  uint64_t indx = m >> 58;

  uint64_t c0 = static_cast<uint64_t>(RSQRT_COEFFS[indx][0]);
  c0 <<= 31;        // to 64 bit with the space for the implicit bit
  c0 |= 1ull << 63; // add implicit bit

  uint64_t c1 = static_cast<uint64_t>(RSQRT_COEFFS[indx][1]);
  c1 <<= 25; // to 64 bit format

  uint64_t c2 = static_cast<uint64_t>(RSQRT_COEFFS[indx][2]);
  uint64_t c3 = static_cast<uint64_t>(RSQRT_COEFFS[indx][3]);

  uint64_t d = (m << 6) >> 32; // local coordinate in the subrange [0, 2^32]
  uint64_t d2 = (d * d) >> 32; // square of the local coordinate
  uint64_t re = c0 + (d2 * c2 >> 13); // even part of the polynomial (positive)
  uint64_t ro = d * ((c1 + ((d2 * c3) >> 19)) >> 26) >>
                6;      // odd part of the polynomial (negative)
  uint64_t r = re - ro; // maximal error < 1.55e-10 and it is less than 2^-32
  // Newton-Raphson first order step to improve accuracy of the result to almost
  // 64 bits.
  r = rsqrt_newton_raphson(m, r);
  // Adjust in the unlucky case x~1;
  if (LIBC_UNLIKELY(!r))
    --r;
  return r;
}
#endif // LIBC_MATH_HAS_SMALL_TABLES

} // namespace sqrtf128_internal

LIBC_INLINE float128 sqrtf128(float128 x) {
  using namespace sqrtf128_internal;
  using FPBits = fputil::FPBits<float128>;
  // Get rounding mode.
  uint32_t rm = fputil::get_round();

  FPBits xbits(x);
  UInt128 x_u = xbits.uintval();
  // Bring leading bit of the mantissa to the highest bit.
  //   ulp(x_frac) = 2^-128.
  UInt128 x_frac = xbits.get_mantissa() << (FPBits::EXP_LEN + 1);

  int sign_exp = static_cast<int>(x_u >> FPBits::FRACTION_LEN);

  if (LIBC_UNLIKELY(sign_exp == 0 || sign_exp >= 0x7fff)) {
    // Special cases: NAN, inf, negative numbers
    if (sign_exp >= 0x7fff) {
      // x = -0 or x = inf
      if (xbits.is_zero() || xbits == xbits.inf())
        return x;
      // x is nan
      if (xbits.is_nan()) {
        // pass through quiet nan
        if (xbits.is_quiet_nan())
          return x;
        // transform signaling nan to quiet and return
        return xbits.quiet_nan().get_val();
      }
      // x < 0 or x = -inf
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
      return xbits.quiet_nan().get_val();
    }
    // Now x is subnormal or x = +0.

    // x is +0.
    if (x_frac == 0)
      return x;

    // Normalize subnormal inputs.
    sign_exp = -cpp::countl_zero(x_frac);
    int normal_shifts = 1 - sign_exp;
    x_frac <<= normal_shifts;
  }

  // For sign_exp = biased exponent of x = real_exponent + 16383,
  // let f be the real exponent of the output:
  //   f = floor(real_exponent / 2)
  // Then:
  //   floor((sign_exp + 1) / 2) = f + 8192
  // Hence, the biased exponent of the final result is:
  //   f + 16383 = floor((sign_exp + 1) / 2) + 8191.
  // Since the output mantissa will include the hidden bit, we can define the
  // output exponent part:
  //   e2 = floor((sign_exp + 1) / 2) + 8190
  unsigned i = static_cast<unsigned>(1 - (sign_exp & 1));
  uint32_t q2 = (sign_exp + 1) >> 1;
  // Exponent of the final result
  uint32_t e2 = q2 + 8190;

  constexpr uint64_t RSQRT_2[2] = {~0ull,
                                   0xb504f333f9de6484 /* 2^64/sqrt(2) */};

  // Approximate 1/sqrt(1 + x_frac)
  // Error: |r_1 - 1/sqrt(x)| < 2^-62.
  uint64_t r1 = rsqrt_approx(static_cast<uint64_t>(x_frac >> 64));
  // Adjust for the even/odd exponent.
  uint64_t r2 = prod_hi(r1, RSQRT_2[i]);
  unsigned shift = 2 - i;

  // Normalized input:
  //   1 <= x_reduced < 4
  UInt128 x_reduced = (x_frac >> shift) | (UInt128(1) << (126 + i));
  // With r2 ~ 1/sqrt(x) up to 2^-63, we perform another round of Newton-Raphson
  // iteration:
  //   r3 = r2 - r2 * h / 2,
  // for h = r2^2 * x - 1.
  // Then:
  //   sqrt(x) = x * (1 / sqrt(x))
  //           ~ x * r3
  //           = x * (r2 - r2 * h / 2)
  //           = (x * r2) - (x * r2) * h / 2
  UInt128 sx = prod_hi(x_reduced, r2);
  UInt128 h = prod_hi(sx, r2) << 2;
  UInt128 ds = static_cast<UInt128>(prod_hi(static_cast<Int128>(h), sx));
  UInt128 v = (sx << 1) - ds;

  uint32_t nrst = rm == FE_TONEAREST;
  // The result lies within (-2,5) of true square root so we now
  // test that we can correctly round the result taking into account
  // the rounding mode.
  // Check the lowest 14 bits (by clearing and sign-extending the top
  // 32 - 14 = 18 bits).
  int dd = (static_cast<int>(v) << 18) >> 18;

  if (LIBC_UNLIKELY(dd < 4 && dd >= -8)) { // can round correctly?
    // m is almost the final result it can be only 1 ulp off so we
    // just need to test both possibilities. We square it and
    // compare with the initial argument.
    UInt128 m = v >> 15;
    UInt128 m2 = m * m;
    // The difference of the squared result and the argument
    Int128 t0 = static_cast<Int128>(m2 - (x_reduced << 98));
    if (t0 == 0) {
      // the square root is exact
      v = m << 15;
    } else {
      // Add +-1 ulp to m depend on the sign of the difference. Here
      // we do not need to square again since (m+1)^2 = m^2 + 2*m +
      // 1 so just need to add shifted m and 1.
      Int128 t1 = t0;
      Int128 sgn = t0 >> 127; // sign of the difference
      Int128 m_xor_sgn = static_cast<Int128>(m << 1) ^ sgn;
      t1 -= m_xor_sgn;
      t1 += Int128(1) + sgn;

      Int128 sgn1 = t1 >> 127;
      if (LIBC_UNLIKELY(sgn == sgn1)) {
        t0 = t1;
        v -= sgn << 15;
        t1 -= m_xor_sgn;
        t1 += Int128(1) + sgn;
      }

      if (t1 == 0) {
        // 1 ulp offset brings again an exact root
        v = (m - static_cast<UInt128>((sgn << 1) + 1)) << 15;
      } else {
        t1 += t0;
        Int128 side = t1 >> 127; // select what is closer m or m+-1
        v &= ~UInt128(0) << 15;  // wipe the fractional bits
        v -= ((sgn & side) | (~sgn & 1)) << (15 + static_cast<int>(side));
        v |= 1; // add sticky bit since we cannot have an exact mid-point
                // situation
      }
    }
  }

  unsigned frac = static_cast<unsigned>(v) & 0x7fff; // fractional part
  unsigned rnd = 0;                                  // round bit
  if (LIBC_LIKELY(nrst != 0)) {
    rnd = frac >> 14; // round to nearest tie to even
  } else if (rm == FE_UPWARD) {
    rnd = !!frac; // round up
  } else {
    rnd = 0; // round down or round to zero
  }

  v >>= 15; // position mantissa
  v += rnd; // round

  // Set inexact flag only if square root is inexact
  // TODO: We will have to raise FE_INEXACT most of the time, but this
  // operation is very costly, especially in x86-64, since technically, it
  // needs to synchronize both SSE and x87 flags.  Need to investigate
  // further to see how we can make this performant.
  // https://github.com/llvm/llvm-project/issues/126753

  // if(frac) fputil::raise_except_if_required(FE_INEXACT);

  v += static_cast<UInt128>(e2) << FPBits::FRACTION_LEN; // place exponent
  return cpp::bit_cast<float128>(v);
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_SQRTF128_H
