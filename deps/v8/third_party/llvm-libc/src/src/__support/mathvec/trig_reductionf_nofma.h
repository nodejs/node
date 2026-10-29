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
/// functions for targets with no FMA support.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_TRIG_REDUCTIONF_NOFMA_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_TRIG_REDUCTIONF_NOFMA_H

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
  constexpr cpp::simd<double, N> inv_pi_hi = 0x1.45f306e000000p-2;
  constexpr cpp::simd<double, N> inv_pi_mid = -0x1.b1bbead000000p-33;
  constexpr cpp::simd<double, N> inv_pi_lo = -0x1.80f62a0b82b00p-63;
  constexpr cpp::simd<double, N> shift = 0x1.8p52;

  // Since we know the input was cast to FP64 from FP32, we can use split
  // 1 / pi coefficients such that the products of hi and mid are exact.
  // The low part restores the remaining precision without requiring an FMA.
  cpp::simd<double, N> x_hi = x * inv_pi_hi;
  cpp::simd<double, N> x_mid = x * inv_pi_mid;
  cpp::simd<double, N> x_lo = x * inv_pi_lo;

  // Adding shift rounds x / pi to the nearest integer k, producing
  // z = shift + k. Therefore t = shift - z = -k.
  cpp::simd<double, N> z = shift + (x_hi + x_mid);
  cpp::simd<double, N> t = shift - z;

  // Accumulate the split products from largest to smallest to recover
  // r = x / pi - k.
  cpp::simd<double, N> r = t + x_hi + x_mid + x_lo;

  // The low bit of z's representation encodes the parity of k.
  return {r, cpp::bit_cast<cpp::simd<int64_t, N>>(z)};
}

// Three-double expansions of 2^(8*q) / pi, reduced modulo an even integer,
// q = 3..12. Padded to 16 length arrays for safe masked indexing.
LIBC_INLINE_VAR constexpr double INV_PI_HI[16] = {
    0,
    0,
    0,
    -0x1.236377d000000p-2,
    -0x1.b1bbead000000p-1,
    -0x1.bbead60000000p-1,
    0x1.529fc27000000p-5,
    0x1.29fc275000000p-1,
    0x1.fc2757d000000p-1,
    0x1.3abe8fb000000p-4,
    -0x1.505c159000000p-2,
    -0x1.7056591000000p-4,
    0x1.f534ddc000000p-1,
    0,
    0,
};

LIBC_INLINE_VAR constexpr double INV_PI_MID[16] = {
    0,
    0,
    0,
    -0x1.6b01ec5000000p-32,
    -0x1.80f62a1000000p-31,
    -0x1.ec54170000000p-32,
    0x1.5f47d4d000000p-35,
    0x1.f47d4d3000000p-31,
    0x1.f534ddc000000p-33,
    -0x1.96447e5000000p-34,
    -0x1.911f925000000p-32,
    -0x1.f924eb5000000p-36,
    0x1.b6c52b3000000p-34,
    0,
    0,
};

LIBC_INLINE_VAR constexpr double INV_PI_LO[16] = {
    0,
    0,
    0,
    -0x1.05c1596447e50p-62,
    0x1.1f534ddc0db80p-61,
    -0x1.596447e493ae0p-62,
    0x1.bb81b6c52b340p-66,
    0x1.dc0db62959940p-61,
    0x1.b6c52b3278800p-66,
    0x1.b14acc9e21c80p-64,
    0x1.4acc9e21c8200p-64,
    -0x1.9b0ef1bef8000p-67,
    0x1.3c439041fe400p-65,
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
  // Since 1023 + 25 = 8 * 131, this is (ix >> 55) - 131.
  cpp::simd<int64_t, N> q =
      cpp::simd_cast<int64_t>(ix >> 55) - cpp::simd<int64_t, N>(131);

  // While sufficiently large x will always produce q within [3, 12],
  // not all input lanes are guaranteed to require the large reduction,
  // so we mask with 15 to keep all values within bounds.
  cpp::simd<int64_t, N> idx = q & cpp::simd<int64_t, N>(15);
  cpp::simd<double, N> c_hi =
      cpp::gather<cpp::simd<double, N>>(true, idx, INV_PI_HI);
  cpp::simd<double, N> c_mid =
      cpp::gather<cpp::simd<double, N>>(true, idx, INV_PI_MID);
  cpp::simd<double, N> c_lo =
      cpp::gather<cpp::simd<double, N>>(true, idx, INV_PI_LO);

  // xr = x * 2^(-8*q) is an integer with xr in [2^25, 2^33).
  cpp::simd<uint64_t, N> scale = cpp::bit_cast<cpp::simd<uint64_t, N>>(q) << 55;
  cpp::simd<double, N> xr = cpp::bit_cast<cpp::simd<double, N>>(ix - scale);

  // Multiplication by the high and middle parts is exact because xr retains
  // the 24-bit significand of its FP32 source and each coefficient has at
  // most 29 significant bits. The low part supplies the remaining precision.
  cpp::simd<double, N> x_hi = xr * c_hi;
  cpp::simd<double, N> x_mid = xr * c_mid;
  cpp::simd<double, N> x_lo = xr * c_lo;

  // Add from largest to smallest before using shift to obtain the nearest
  // integer k.
  cpp::simd<double, N> x_sum = (x_hi + x_mid) + x_lo;
  cpp::simd<double, N> biased = shift + x_sum;
  cpp::simd<double, N> kd = biased - shift;

  // Subtract k from the same split expansion to recover r = x / pi - k.
  cpp::simd<double, N> r = x_hi - kd;
  r = r + x_mid;
  r = r + x_lo;

  // The low bit of biased's representation encodes the parity of k.
  return {r, cpp::bit_cast<cpp::simd<int64_t, N>>(biased)};
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_TRIG_REDUCTIONF_NOFMA_H
