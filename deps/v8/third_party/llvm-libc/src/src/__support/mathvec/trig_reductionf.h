//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains range reductions for single-precision trigonometric
/// functions for targets with FMA support.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_TRIG_REDUCTIONF_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_TRIG_REDUCTIONF_H

#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/simd.h"

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

template <size_t N> struct Reduction {
  cpp::simd<double, N> r;
  cpp::simd<int64_t, N> odd;
};

// Reduces x / pi into k + r, with k as an integer and |r| <= 0.5.
template <size_t N>
LIBC_INLINE static Reduction<N> fast_reduction(cpp::simd<double, N> x) {
  constexpr cpp::simd<double, N> inv_pi = 0x1.45f306dc9c883p-2;
  constexpr cpp::simd<double, N> inv_pi_tail = -0x1.6b01ec5417056p-56;
  constexpr cpp::simd<double, N> shift = 0x1.8p52;

  // Adding shift rounds x / pi to the nearest integer k,
  // producing z = shift + k. Therefore t = shift - z = -k.
  cpp::simd<double, N> z = cpp::multiply_add(x, inv_pi, shift);
  cpp::simd<double, N> t = shift - z;

  // r = x/pi - k
  cpp::simd<double, N> r;
  r = cpp::multiply_add(x, inv_pi, t);
  r = cpp::multiply_add(x, inv_pi_tail, r);

  return {r, cpp::bit_cast<cpp::simd<int64_t, N>>(z)};
}

// Two-double expansions of 2^(8*q) / pi reduced modulo an even integer,
// q = 3..12. Padded to a 16 length array for easier access.
LIBC_INLINE_VAR constexpr double INV_PI_HI[16] = {
    0,
    0,
    0,
    -0x1.236377d5ac07bp-2,
    -0x1.b1bbead603d8bp-1,
    -0x1.bbead603d8a83p-1,
    0x1.529fc2757d1f5p-5,
    0x1.29fc2757d1f53p-1,
    0x1.fc2757d1f534ep-1,
    0x1.3abe8fa9a6eep-4,
    -0x1.505c1596447e5p-2,
    -0x1.70565911f924fp-4,
    0x1.f534ddc0db629p-1,
    0,
    0,
    0,
};

LIBC_INLINE_VAR constexpr double INV_PI_LO[16] = {
    0,
    0,
    0,
    -0x1.505c1596447e5p-58,
    0x1.f47d4d377036ep-55,
    0x1.f534ddc0db629p-57,
    0x1.a6ee06db14acdp-60,
    0x1.377036d8a5665p-55,
    -0x1.1f924eb53361ep-56,
    0x1.b6c52b3278872p-58,
    0x1.b14acc9e21c82p-56,
    0x1.2b32788720840p-58,
    0x1.664f10e4107f9p-55,
    0,
    0,
    0,
};

// Reduces non-negative large finite inputs x >= 0x1p49.
// Decomposes x / pi into k + r, with k as an integer and |r| <= 0.5.
template <size_t N>
LIBC_INLINE static Reduction<N> large_reduction(cpp::simd<double, N> x) {
  constexpr cpp::simd<double, N> shift = 0x1.8p52;

  cpp::simd<uint64_t, N> ix = cpp::bit_cast<cpp::simd<uint64_t, N>>(x);

  // Compute q = floor((unbiased_exponent - 25) / 8).
  // Since 1023 + 25 = 8 * 131, this can be calculated as (ix >> 55) - 131.
  cpp::simd<int64_t, N> q =
      cpp::simd_cast<int64_t>(ix >> 55) - cpp::simd<int64_t, N>(131);

  // While sufficiently large x will always produce q within [3, 12],
  // not all input lanes are guaranteed to require the large reduction,
  // so we mask with 15 to keep all values within bounds.
  cpp::simd<int64_t, N> idx = q & cpp::simd<int64_t, N>(15);
  cpp::simd<double, N> c_hi =
      cpp::gather<cpp::simd<double, N>>(true, idx, INV_PI_HI);
  cpp::simd<double, N> c_lo =
      cpp::gather<cpp::simd<double, N>>(true, idx, INV_PI_LO);

  // xr = x * 2^(-8*q) is an integer with xr in [2^25, 2^33).
  cpp::simd<uint64_t, N> scale = cpp::bit_cast<cpp::simd<uint64_t, N>>(q) << 55;
  cpp::simd<double, N> xr = cpp::bit_cast<cpp::simd<double, N>>(ix - scale);

  // Subtract the nearest integer from xr * c_hi, and add the tail.
  cpp::simd<double, N> biased = cpp::multiply_add(xr, c_hi, shift);
  cpp::simd<double, N> kd = biased - shift;
  cpp::simd<double, N> r = cpp::multiply_add(xr, c_hi, -kd);
  r = cpp::multiply_add(xr, c_lo, r);

  return {r, cpp::bit_cast<cpp::simd<int64_t, N>>(biased)};
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_TRIG_REDUCTIONF_H
