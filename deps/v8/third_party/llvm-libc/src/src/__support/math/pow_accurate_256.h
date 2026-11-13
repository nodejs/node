//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// 256-bit accurate path for double-precision pow(x, y).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_POW_ACCURATE_256_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_POW_ACCURATE_256_H

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/big_int.h"
#include "src/__support/common.h"
#include "src/__support/frac128.h"
#include "src/__support/frac256.h"
#include "src/__support/frac64.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/math/pow_utils.h"
#include "src/__support/math_extras.h"
#include "src/__support/sign.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace pow_internal {

using DFloat256 = typename fputil::DyadicFloat<256>;

// Lookup table for -log2(R_1[i]) for i = 0..16 as Frac256 (lsb = 2^-255),
// where R_1[i] = RD_SMALL_K[i] * 2^-6.
LIBC_INLINE_VAR constexpr Frac256 LOG2_R1_FRAC256[17] = {
    // lsb = 2^-255
    Frac256({0x0000000000000000ULL, 0x0000000000000000ULL,
             0x0000000000000000ULL, 0x0000000000000000ULL}), // i = 0
    Frac256({0x9e9fded86733d3caULL, 0xbaad60b1bf6bcd42ULL,
             0x9da288fc615a727dULL, 0x0beb024b67dda633ULL}), // i = 1
    Frac256({0x6816ff4ab1e1af50ULL, 0x20909dd35a2a212aULL,
             0x8720de5377caf325ULL, 0x1563dc29ffacb20cULL}), // i = 2
    Frac256({0x4627a78c47089d0dULL, 0x4892260a8c79779fULL,
             0x8fef64f0f62e7b46ULL, 0x1f5fd8a9063e3490ULL}), // i = 3
    Frac256({0x9d7b13b46dda1849ULL, 0x885dce30d5e9e813ULL,
             0xc9d0b5ca5a8e7bb5ULL, 0x29edf7659d8b30f1ULL}), // i = 4
    Frac256({0x4429d9e262f3c8c6ULL, 0xc7395ab7fe8e9835ULL,
             0xd536fc5bec1a57b8ULL, 0x315130157f7a64ccULL}), // i = 5
    Frac256({0xe91dc50aab435aa9ULL, 0xe06bcb114a0f1719ULL,
             0xf6a2b59276887aabULL, 0x3cfbebfca715669dULL}), // i = 6
    Frac256({0x3ad4311182915175ULL, 0x60c67a245f78bb52ULL,
             0x8283ccdf555594a0ULL, 0x4531583f9a2be203ULL}), // i = 7
    Frac256({0xe61b71e474e99f75ULL, 0x75abfd08dec73401ULL,
             0xd7220e04ebb0e2a4ULL, 0x497028118efabeb7ULL}), // i = 8
    Frac256({0x2e147034099ccfcfULL, 0xb03b7d7e6bd0493fULL,
             0xe55be97611f87779ULL, 0x523bbc64c5e64350ULL}), // i = 9
    Frac256({0xf662658135c18184ULL, 0x5f0bcac4e6cfec7bULL,
             0x748d68b767f88088ULL, 0x5b77f0a9e3f18cfbULL}), // i = 10
    Frac256({0xa609c77144ded04cULL, 0xb30a91252b56f94aULL,
             0x572667587b10ca0dULL, 0x6043e946fd97f5dcULL}), // i = 11
    Frac256({0x5509d9aded608bccULL, 0xdf7d4115ebc0ee87ULL,
             0x1ee1343fe7c9cb4aULL, 0x6530a1d24b136c10ULL}), // i = 12
    Frac256({0x7ea793f02baad929ULL, 0xb0c4015f8fdff17dULL,
             0xd843902f5aad7542ULL, 0x6f73a77325861c69ULL}), // i = 13
    Frac256({0xdb6ddbdb00d73945ULL, 0x1ad7c182a716c033ULL,
             0x99d63ecf5dd4529eULL, 0x74ce04829b7674c1ULL}), // i = 14
    Frac256({0xfce168eaef943079ULL, 0xce4c86d28e4be331ULL,
             0xb27e43da520fbdb7ULL, 0x7a514b229c409e33ULL}), // i = 15
    Frac256({0x0000000000000000ULL, 0x0000000000000000ULL,
             0x0000000000000000ULL, 0x8000000000000000ULL}), // i = 16
};

// Lookup table for -log2(R_2[k2]) for k2 = -4..4 as two's-complement Frac256
// (lsb = 2^-255), where R_2[k2] = (128 - k2) * 2^-7.
LIBC_INLINE_VAR constexpr Frac256 LOG2_R2_FRAC256[9] = {
    // lsb = 2^-255
    Frac256({0xfce168eaef943079ULL, 0xce4c86d28e4be331ULL,
             0xb27e43da520fbdb7ULL, 0xfa514b229c409e33ULL}), // k2 = -4
    Frac256({0x59576b46a119898aULL, 0x254008aeb4167a33ULL,
             0x9faebec15b2e2b43ULL, 0xfbb8cb8abb32fb44ULL}), // k2 = -3
    Frac256({0x10e7d4a6c2ef9745ULL, 0x3ead121a489569ffULL,
             0xb23b03bdcfdea0d7ULL, 0xfd230d2f47a5baceULL}), // k2 = -2
    Frac256({0xa828a9bde1ec7e79ULL, 0xe33209b70d9a5be1ULL,
             0x071c84ffe86b0bbbULL, 0xfe901af4910f7ae8ULL}), // k2 = -1
    Frac256({0x0000000000000000ULL, 0x0000000000000000ULL,
             0x0000000000000000ULL, 0x0000000000000000ULL}), // k2 = 0
    Frac256({0x6e26f44a7ba5ed8aULL, 0xd4a6b5a62ff68790ULL,
             0xb5d184a2c615b70aULL, 0x0172c7ba20f73275ULL}), // k2 = 1
    Frac256({0x262f5ca40b7fa26cULL, 0x3ea8c6b85ced9bdaULL,
             0xca906c23ef817e0bULL, 0x02e87dd0c3e6aac6ULL}), // k2 = 2
    Frac256({0x95b7f4fcee92de52ULL, 0xe775fc0ab1c9f028ULL,
             0x48f836042de0dc32ULL, 0x04612e39315abe0aULL}), // k2 = 3
    Frac256({0xa3a1e195dfe2d23bULL, 0xfc9f0716c9c77bc6ULL,
             0x7970e03f821c75d5ULL, 0x05dce53276563557ULL}), // k2 = 4
};

// Polynomial approximation for log2(1 + dx):
// For |dx| <= 131 / 32768 < 2^-7.96, we approximate:
//   P(x) ~ log2(1 + x) / (2 * x)
// then:
//   log2(1 + dx) = 2 * dx * P(dx).
//
// Minimax polynomial coefficients generated by Sollya with:
// > prec = 512;
// > P = fpminimax(log2(1 + x) / (2 * x), 28, [|256...|], [-131/32768,
//                 131/32768], fixed);
// > dirtyinfnorm(log2(1 + x) - 2 * x * P, [-131/32768, 131/32768]);
//   0x1.f78ee...p-271 < 2^-270.
//
// Coefficients are stored in 3 precision tiers matching their Horner stages:
// - Degrees 0..7 in Frac256 (lsb = 2^-256)
// - Degrees 8..16 in Frac128 (lsb = 2^-128)
// - Degrees 17..28 in Frac64 (lsb = 2^-64)
LIBC_INLINE_VAR constexpr Frac256 LOG2_POLY_256[8] = {
    // lsb = 2^-256
    Frac256({0x8b25166cd1a13248ULL, 0xeb577aa8dd695a58ULL,
             0xbe87fed0691d3e88ULL, 0xb8aa3b295c17f0bbULL}), // a_0
    Frac256({0x45928b3668d0990fULL, 0x75abbd546eb4ad2cULL,
             0xdf43ff68348e9f44ULL, 0x5c551d94ae0bf85dULL}), // a_1
    Frac256({0x2e61b22445ceb3f3ULL, 0x4e727e3849cdc8c8ULL,
             0x3f82aa45785f14d8ULL, 0x3d8e13b87407fae9ULL}), // a_2
    Frac256({0x22c9459b3f6eb564ULL, 0x3ad5deaa375a5696ULL,
             0xefa1ffb41a474fa2ULL, 0x2e2a8eca5705fc2eULL}), // a_3
    Frac256({0x4f076da17511bc00ULL, 0x2f117eee92aeababULL,
             0x594e6629ae9f72e8ULL, 0x24eed8a1df37fcf2ULL}), // a_4
    Frac256({0x172f21933a77f0c1ULL, 0x27393f1c24e6e464ULL,
             0x9fc15522bc2f8a6cULL, 0x1ec709dc3a03fd74ULL}), // a_5
    Frac256({0x05837685e5671b1aULL, 0x219ec86144339f31ULL,
             0x645c921dc5df9b38ULL, 0x1a61762a7aded93fULL}), // a_6
    Frac256({0xfa3f8f34cb3f2ee5ULL, 0x1d6aef551bad2b6aULL,
             0x77d0ffda0d23a7d1ULL, 0x171547652b82fe17ULL}), // a_7
};

LIBC_INLINE_VAR constexpr Frac128 LOG2_POLY_128[9] = {
    // lsb = 2^-128
    Frac128({0x6a80e36c7d7506f2ULL, 0x1484b13d7c02a8f8ULL}), // a_8
    Frac128({0x2ca73314d74fb974ULL, 0x12776c50ef9bfe79ULL}), // a_9
    Frac128({0x5723a2cd20d41cf5ULL, 0x10c9a84994022d28ULL}), // a_10
    Frac128({0x4fe0aa915e17c536ULL, 0x0f6384ee1d01febaULL}), // a_11
    Frac128({0x711e274b1bc72c31ULL, 0x0e347ab4698bb00eULL}), // a_12
    Frac128({0xb22e490ee2efcd9bULL, 0x0d30bb153d6f6c9fULL}), // a_13
    Frac128({0x731a220de4dfd03eULL, 0x0c4f9d8b4a67fefbULL}), // a_14
    Frac128({0xbbe87fed0692488aULL, 0x0b8aa3b295c17f0bULL}), // a_15
    Frac128({0xa1cbc3b1e9a2722bULL, 0x0adcd64dba1f86a1ULL}), // a_16
};

LIBC_INLINE_VAR constexpr Frac64 LOG2_POLY_64[12] = {
    // lsb = 2^-64
    Frac64(0x0a42589ebe01547cULL), // a_17
    Frac64(0x09b81e0fa687ff32ULL), // a_18
    Frac64(0x093bb62877cdff3cULL), // a_19
    Frac64(0x08cb27637e4a486aULL), // a_20
    Frac64(0x0864d424ca011692ULL), // a_21
    Frac64(0x080766bf04010878ULL), // a_22
    Frac64(0x07b1c2770e823fbfULL), // a_23
    Frac64(0x0762f82060d60050ULL), // a_24
    Frac64(0x071a3d5995afd976ULL), // a_25
    Frac64(0x06d6e5744c41469bULL), // a_26
    Frac64(0x06988c480b11e361ULL), // a_27
    Frac64(0x06681a21fa0d204cULL), // a_28
};

// Polynomial coefficients c_k ~ (log(2))^(k+1) / (k+1)! for 2^u - 1 = u * P(u)
// on |u| < 2^-7.29, stored in 2 precision tiers:
// - Degrees 0..9 in Frac256 (lsb = 2^-256)
// - Degrees 10..19 in Frac128 (lsb = 2^-128)
LIBC_INLINE_VAR constexpr Frac256 EXP2_POLY_256[10] = {
    // lsb = 2^-256
    Frac256({0x8a0d175b8baafa2cULL, 0x40f343267298b62dULL,
             0xc9e3b39803f2f6afULL, 0xb17217f7d1cf79abULL}), // c_0
    Frac256({0x857d27776272dcddULL, 0x4744ea38619cd3a9ULL,
             0xde2d60dd92e6bf95ULL, 0x3d7f7bff058b1d50ULL}), // c_1
    Frac256({0x8ea63315bc46c28dULL, 0x4f5c47444da0110eULL,
             0x99d3b15d995e96f7ULL, 0x0e35846b82505fc5ULL}), // c_2
    Frac256({0xd336e12d33c302bfULL, 0xe48f1d4a7cc7223aULL,
             0x39977c16a7dd58a0ULL, 0x0276556df749cee5ULL}), // c_3
    Frac256({0x919b86d5480c127eULL, 0xc15db29a5b9c65c3ULL,
             0x41c5fda69452fb0cULL, 0x005761ff9e299cc4ULL}), // c_4
    Frac256({0xa92b25fedad56f0dULL, 0x9f6629ff9988f760ULL,
             0xb7a58544c3591a0fULL, 0x000a184897c363c3ULL}), // c_5
    Frac256({0xba623264ff59063bULL, 0x959c22a5d1021fdcULL,
             0x34358a8e643ec734ULL, 0x0000ffe5fe2c4586ULL}), // c_6
    Frac256({0xfab6439a0b2d00d1ULL, 0x4b0dc341ee20f572ULL,
             0x23fd8ffe606da77cULL, 0x0000162c0223a5c8ULL}), // c_7
    Frac256({0xe715fa54922e6ef4ULL, 0x08a319719553744cULL,
             0x7c3da4a70e5a4ff9ULL, 0x000001b5253d395eULL}), // c_8
    Frac256({0x308d164959a6077dULL, 0x699c540c1142cae1ULL,
             0x8ec9f6fda1d952e7ULL, 0x0000001e4cf5158bULL}), // c_9
};

LIBC_INLINE_VAR constexpr Frac128 EXP2_POLY_128[10] = {
    // lsb = 2^-128
    Frac128({0x1bb24c0f57995e47ULL, 0x00000001e8cac735ULL}), // c_10
    Frac128({0xfc2985e2b5687e18ULL, 0x000000001c3bd650ULL}), // c_11
    Frac128({0x166d0f96281ac301ULL, 0x0000000001816193ULL}), // c_12
    Frac128({0x4d5878a973f14362ULL, 0x0000000000131496ULL}), // c_13
    Frac128({0x421d82010f33d8acULL, 0x000000000000e1b7ULL}), // c_14
    Frac128({0x44d73cfc59c91c7fULL, 0x00000000000009c7ULL}), // c_15
    Frac128({0x1112d070969f5587ULL, 0x0000000000000066ULL}), // c_16
    Frac128({0xee2ed7b686d2bae6ULL, 0x0000000000000003ULL}), // c_17
    Frac128({0x24b4fd9706b8c9b6ULL, 0x0000000000000000ULL}), // c_18
    Frac128({0x0145acc4b5051358ULL, 0x0000000000000000ULL}), // c_19
};

// Exact 2-step integer range reduction for m_x = 1 + x_mant * 2^-52 in [1, 2):
//   idx1 = round(x_mant * 2^-48) in [0, 16],  r1 = RD_SMALL_K[idx1] * 2^-6
//   d1   = r1 * m_x - 1 = (mant_x * RD_SMALL_K[idx1] - 2^58) * 2^-58
//   k2   = round(d1 * 2^7) in [-4, 4],        r2 = (128 - k2) * 2^-7
//   dx   = r1 * r2 * m_x - 1 = dx_int * 2^-65, with |dx| < 2^-7.96.
LIBC_INLINE int64_t log2_range_reduce(uint64_t x_mant, unsigned &idx1,
                                      int &k2) {
  uint64_t mant_x = x_mant | (1ULL << 52); // lsb = 2^-52
  idx1 = static_cast<unsigned>((x_mant + (1ULL << 47)) >> 48);
  int64_t d1 = static_cast<int64_t>(mant_x * RD_SMALL_K[idx1]) -
               (1LL << 58);                                      // lsb = 2^-58
  k2 = static_cast<int>((d1 + (1LL << 50)) >> 51);               // lsb = 2^-7
  return (d1 << 7) - (static_cast<int64_t>(k2) << 58) - d1 * k2; // lsb = 2^-65
}

// Multiply a in-place by 64-bit b and return the high 64-bit word.
template <size_t N>
LIBC_ALWAYS_INLINE uint64_t mul_uint64(UInt<N> &a, uint64_t b) {
  uint64_t carry = 0;
  for (size_t i = 0; i < N / 64; ++i) {
    auto prod = multiword::mul2(a.val[i], b);
    uint64_t c = 0;
    a.val[i] = add_with_carry<uint64_t>(prod[0], carry, 0, c);
    carry = prod[1] + c;
  }
  return carry;
}

// Construct a normalized DyadicFloat<N> from a mantissa whose MSB is at bit
// N - 1 or N - 2 (at most a 1-bit left shift).
template <size_t N>
LIBC_ALWAYS_INLINE fputil::DyadicFloat<N> df_norm_1bit(Sign sign, int exp,
                                                       const UInt<N> &m) {
  fputil::DyadicFloat<N> r;
  r.sign = sign;
  if (m.val[N / 64 - 1] >> 63) {
    r.exponent = exp;
    r.mantissa = m;
  } else {
    r.exponent = exp - 1;
    r.mantissa = m + m;
  }
  return r;
}

// Convert an int to a normalized DyadicFloat<N>.
template <size_t N>
LIBC_ALWAYS_INLINE fputil::DyadicFloat<N> df_from_int(int x) {
  fputil::DyadicFloat<N> r;
  r.mantissa = UInt<N>(0);
  if (LIBC_UNLIKELY(x == 0)) {
    r.sign = Sign::POS;
    r.exponent = 0;
    return r;
  }
  uint32_t m = static_cast<uint32_t>(x < 0 ? -static_cast<int64_t>(x) : x);
  int lz = cpp::countl_zero(m);
  r.sign = (x < 0) ? Sign::NEG : Sign::POS;
  r.exponent = 31 - lz - static_cast<int>(N - 1);
  r.mantissa.val[N / 64 - 1] = static_cast<uint64_t>(m) << (lz + 32);
  return r;
}

// Combine integer exponent x_e and fractional mantissa log2_m in (0, 1)
// (lsb = 2^-(N - 1)) into a normalized DyadicFloat<N>.
template <size_t N>
LIBC_INLINE fputil::DyadicFloat<N> combine_log2_df(int x_e,
                                                   const UInt<N> &log2_m) {
  Sign sign = Sign::POS;
  uint32_t m = static_cast<uint32_t>(x_e);
  UInt<N> frac = log2_m;
  if (x_e < 0) {
    sign = Sign::NEG;
    m = static_cast<uint32_t>(-x_e - 1);
    frac = (UInt<N>(1) << (N - 1)) - log2_m;
  }

  fputil::DyadicFloat<N> r;
  r.sign = sign;
  if (m == 0) {
    // Near-1.0 inputs already returned early, so frac > 2^(N - 9) and lz is
    // strictly in [1, 8].
    int lz = cpp::countl_zero(frac.val[N / 64 - 1]);
    int rsh = 64 - lz;
    r.exponent = -static_cast<int>(N - 1) - lz;
    r.mantissa.val[0] = frac.val[0] << lz;
    LIBC_LOOP_NOUNROLL
    for (size_t i = 1; i < N / 64; ++i)
      r.mantissa.val[i] = (frac.val[i] << lz) | (frac.val[i - 1] >> rsh);
    return r;
  }
  int bw = 32 - cpp::countl_zero(m);
  r.exponent = -static_cast<int>(N) + bw;
  if (bw == 1) {
    r.mantissa = frac;
    r.mantissa.val[N / 64 - 1] |= 1ULL << 63;
    return r;
  }
  int shr = bw - 1;
  int shl = 64 - shr;
  LIBC_LOOP_NOUNROLL
  for (size_t i = 0; i < N / 64 - 1; ++i)
    r.mantissa.val[i] = (frac.val[i] >> shr) | (frac.val[i + 1] << shl);
  r.mantissa.val[N / 64 - 1] =
      (frac.val[N / 64 - 1] >> shr) | (static_cast<uint64_t>(m) << (64 - bw));
  return r;
}

// Accurate log2(x) in 256-bit precision using 2-step integer range reduction:
//   x = 2^x_e * m_x, with 1 <= m_x < 2
//   log2(x) = x_e + (-log2(r1)) + (-log2(r2)) + log2(1 + dx).
// log2(1 + dx) is evaluated with a degree-28 minimax polynomial using a 3-tier
// Horner scheme (Frac64, Frac128, and Frac256).
LIBC_COLD LIBC_INLINE DFloat256 log2_f256(int x_e, uint64_t x_mant) {
  if (LIBC_UNLIKELY(x_mant == 0))
    return df_from_int<256>(x_e);

  unsigned idx1 = 0;
  int k2 = 0;
  int64_t dx_int = log2_range_reduce(x_mant, idx1, k2); // lsb = 2^-65
  bool dx_is_neg = (dx_int < 0);
  uint64_t abs_dx =
      static_cast<uint64_t>(dx_is_neg ? -dx_int : dx_int); // lsb = 2^-65

  Frac64 y64(abs_dx >> 1);                      // lsb = 2^-64
  Frac128 y128({abs_dx << 63, abs_dx >> 1});    // lsb = 2^-128
  Frac256 y({0, 0, abs_dx << 63, abs_dx >> 1}); // lsb = 2^-256

  // Step 1: 64-bit evaluation for degrees 17..28 (|dx|^17 < 2^-135.4):
  Frac64 p64 = LOG2_POLY_64[11]; // lsb = 2^-64
  LIBC_LOOP_NOUNROLL
  for (int k = 10; k >= 0; --k) {
    Frac64 py = p64 * y64;
    p64 = dx_is_neg ? (LOG2_POLY_64[k] + py)
                    : (LOG2_POLY_64[k] - py); // lsb = 2^-64
  }

  // Step 2: 128-bit evaluation for degrees 8..16 (|dx|^8 < 2^-63.7):
  Frac128 p128({0, p64.val[0]}); // lsb = 2^-128
  LIBC_LOOP_NOUNROLL
  for (int k = 8; k >= 0; --k) {
    Frac128 py = p128 * y128;
    p128 = dx_is_neg ? (LOG2_POLY_128[k] + py)
                     : (LOG2_POLY_128[k] - py); // lsb = 2^-128
  }

  // Step 3: 256-bit evaluation for degrees 0..7:
  Frac256 p({0, 0, p128.val[0], p128.val[1]}); // lsb = 2^-256
  LIBC_LOOP_NOUNROLL
  for (int k = 7; k >= 0; --k) {
    Frac256 py = p * y;
    p = dx_is_neg ? (LOG2_POLY_256[k] + py)
                  : (LOG2_POLY_256[k] - py); // lsb = 2^-256
  }

  // Normalize abs_dx before multiplying by p so near-1.0 inputs (x_e - log2(r)
  // == 0) retain full 256-bit precision without shifting out low bits.
  int lz = cpp::countl_zero(abs_dx);
  uint64_t norm_dx = abs_dx << lz;                        // lsb = 2^-(65 + lz)
  Frac256 norm_log2_1p = Frac256({0, 0, 0, norm_dx}) * p; // lsb = 2^-(256 + lz)
  if (LIBC_UNLIKELY((x_e == 0 && idx1 == 0 && k2 == 0) ||
                    (x_e == -1 && idx1 == 16 && k2 == 0))) {
    return df_norm_1bit<256>(dx_is_neg ? Sign::NEG : Sign::POS, -256 - lz,
                             UInt<256>(norm_log2_1p.val));
  }

  Frac256 log2_1p = norm_log2_1p >> static_cast<size_t>(lz + 1); // lsb = 2^-255
  Frac256 log2_rd =
      LOG2_R1_FRAC256[idx1] + LOG2_R2_FRAC256[k2 + 4]; // lsb = 2^-255
  Frac256 log2_m =
      dx_is_neg ? (log2_rd - log2_1p) : (log2_rd + log2_1p); // lsb = 2^-255

  return combine_log2_df<256>(x_e, UInt<256>(log2_m.val));
}

// Accurate 2^z in 256-bit precision reusing LOG2_R1_FRAC256 and LOG2_R2_FRAC256
// for range reduction:
//   z = hi - f, with hi = ceil(z) and f in [0, 1)
//   f ~ -log2(R_1[idx1] * R_2[k2])
//   u = (-log2(R_1[idx1]) - log2(R_2[k2])) - f, with |u| < 2^-7.29.
// Then:
//   2^z = 2^(hi - 13) * (RD_SMALL_K[idx1] * (128 - k2)) * 2^u.
LIBC_COLD LIBC_INLINE DFloat256 exp2_f256(const DFloat256 &z) {
  // Callers guarantee 2^-54 <= |z| < 1076 < 2^11, so e in [-54, 10].
  int e = z.exponent + 255;
  int int_z = 0;
  UInt<256> frac_z;
  const auto &m = z.mantissa.val;
  if (e > 0) {
    int shr = 64 - e;
    int_z = static_cast<int>(m[3] >> (shr - 1));
    frac_z = UInt<256>({m[0] << e, (m[1] << e) | (m[0] >> shr),
                        (m[2] << e) | (m[1] >> shr),
                        ((m[3] << e) | (m[2] >> shr)) &
                            0x7fff'ffff'ffff'ffffULL}); // lsb = 2^-255
  } else if (e == 0) {
    int_z = 1;
    frac_z = UInt<256>(
        {m[0], m[1], m[2], m[3] & 0x7fff'ffff'ffff'ffffULL}); // lsb = 2^-255
  } else {
    int shr = -e;
    int shl = 64 - shr;
    frac_z =
        UInt<256>({(m[0] >> shr) | (m[1] << shl), (m[1] >> shr) | (m[2] << shl),
                   (m[2] >> shr) | (m[3] << shl), m[3] >> shr}); // lsb = 2^-255
  }

  int hi = -int_z;
  Frac256 f(frac_z.val); // lsb = 2^-255
  if (z.sign != Sign::NEG) {
    if (frac_z.is_zero()) {
      hi = int_z;
    } else {
      hi = int_z + 1;
      f = Frac256(((UInt<256>(1) << 255) - frac_z).val);
    }
  }

  // Direct integer index approximation for idx1 = round(16 * (2^f - 1)) in
  // [0, 16] and k2 = round((f + log2(R_1[idx1])) * 128 * ln(2)) in [-4, 4]:
  uint64_t q = f.val[3] >> 47; // lsb = 2^-16
  uint64_t p2 = 15512 + ((q * 4561) >> 16);
  uint64_t p1 = 45462 + ((q * p2) >> 16);
  unsigned idx1 = static_cast<unsigned>((q * p1 + (1ULL << 27)) >> 28);

  int64_t rem_hi = static_cast<int64_t>(f.val[3]) -
                   static_cast<int64_t>(LOG2_R1_FRAC256[idx1].val[3]);
  int k2 = static_cast<int>(((rem_hi >> 32) * 5814539LL + (1LL << 46)) >> 47);
  if (k2 < -4)
    k2 = -4;
  if (k2 > 4)
    k2 = 4;

  Frac256 log2_rd =
      LOG2_R1_FRAC256[idx1] + LOG2_R2_FRAC256[k2 + 4]; // lsb = 2^-255
  Frac256 u_signed = log2_rd - f;                      // lsb = 2^-255
  bool u_is_neg = (u_signed.val[3] >> 63) != 0;
  Frac256 abs_u = u_is_neg ? (Frac256(0) - u_signed) : u_signed;
  Frac256 u = abs_u << 1; // lsb = 2^-256

  // Step 1: 128-bit evaluation for degrees 10..19:
  Frac128 u128 = u.to_frac128();   // lsb = 2^-128
  Frac128 p128 = EXP2_POLY_128[9]; // lsb = 2^-128
  LIBC_LOOP_NOUNROLL
  for (int i = 8; i >= 0; --i) {
    Frac128 pu = p128 * u128;
    p128 = u_is_neg ? (EXP2_POLY_128[i] - pu)
                    : (EXP2_POLY_128[i] + pu); // lsb = 2^-128
  }

  // Step 2: 256-bit evaluation for degrees 0..9:
  Frac256 p({0, 0, p128.val[0], p128.val[1]}); // lsb = 2^-256
  LIBC_LOOP_NOUNROLL
  for (int i = 9; i >= 0; --i) {
    Frac256 pu = p * u;
    p = u_is_neg ? (EXP2_POLY_256[i] - pu)
                 : (EXP2_POLY_256[i] + pu); // lsb = 2^-256
  }

  // Reconstruction:
  //   2^z = 2^(hi - 13) * m_int * (1 +- t)
  //       = 2^hi * (m_int * 2^-13) * (1 +- t).
  Frac256 t = u * p; // lsb = 2^-256
  uint64_t m_int =
      static_cast<uint64_t>(RD_SMALL_K[idx1]) * static_cast<uint64_t>(128 - k2);
  UInt<256> m_256({0, 0, 0, m_int << 50}); // lsb = 2^-255
  Frac256 mt = Frac256(m_256.val) * t;     // lsb = 2^-255
  UInt<256> m_final = u_is_neg ? (m_256 - UInt<256>(mt.val))
                               : (m_256 + UInt<256>(mt.val)); // lsb = 2^-255

  return df_norm_1bit<256>(Sign::POS, hi - 255, m_final);
}

// Accurate pow(x, y) evaluating z = y * log2(x) and 2^z in 256-bit precision,
// folded with sticky bit into a normalized DyadicFloat<128> for rounding.
LIBC_COLD LIBC_NOINLINE LIBC_INLINE fputil::DyadicFloat<128>
pow_accurate_256(Sign y_sign, int exp_y, uint64_t mant_y, bool is_neg, int x_e,
                 uint64_t x_mant) {
  DFloat256 log2_x = log2_f256(x_e, x_mant);
  uint64_t z_hi = mul_uint64(log2_x.mantissa, mant_y);
  const auto &lm = log2_x.mantissa.val;

  DFloat256 z;
  z.sign = (y_sign == log2_x.sign) ? Sign::POS : Sign::NEG;
  if (z_hi >> 63) {
    z.exponent = log2_x.exponent + exp_y + 1;
    z.mantissa = UInt<256>({lm[1], lm[2], lm[3], z_hi});
  } else {
    z.exponent = log2_x.exponent + exp_y;
    z.mantissa =
        UInt<256>({(lm[1] << 1) | (lm[0] >> 63), (lm[2] << 1) | (lm[1] >> 63),
                   (lm[3] << 1) | (lm[2] >> 63), (z_hi << 1) | (lm[3] >> 63)});
  }

  DFloat256 r = exp2_f256(z);
  fputil::DyadicFloat<128> r128;
  r128.sign = is_neg ? Sign::NEG : Sign::POS;
  r128.exponent = r.exponent + 128;
  uint64_t sticky = (r.mantissa.val[0] | r.mantissa.val[1]) != 0;
  r128.mantissa = UInt<128>({r.mantissa.val[2] | sticky, r.mantissa.val[3]});
  return r128;
}

} // namespace pow_internal
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_POW_ACCURATE_256_H
