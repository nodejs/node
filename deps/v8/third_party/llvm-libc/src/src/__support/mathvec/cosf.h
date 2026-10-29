//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implemenation for single-precision SIMD cos.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_COSF_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_COSF_H

#include "src/__support/CPP/simd.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/properties/cpu_features.h"

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
#include "src/__support/mathvec/trig_reductionf.h"
#else
#include "src/__support/mathvec/trig_reductionf_nofma.h"
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

template <size_t N>
LIBC_INLINE static cpp::simd<double, N> cospif_poly(cpp::simd<double, N> r) {
  // Approximate cos(pi * r) as (1/4 - r^2) * P(r^2) for |r| <= 0.5.
  // These coefficients aren't produced directly via sollya, but rather
  // are fine-tuned by iterative adjustment to remove hard to round cases.
  // TODO: Create a tool to deterministically reproduce these coefficients.
  // see https://github.com/llvm/llvm-project/issues/220984
  constexpr cpp::simd<double, N> c0 = 0x1p2;
  constexpr cpp::simd<double, N> c1 = -0x1.de9e64df22f07p1;
  constexpr cpp::simd<double, N> c2 = 0x1.472be1223eeadp0;
  constexpr cpp::simd<double, N> c3 = -0x1.d4fcd82b511ebp-3;
  constexpr cpp::simd<double, N> c4 = 0x1.9f05c866286ddp-6;
  constexpr cpp::simd<double, N> c5 = -0x1.f308c07837812p-10;
  constexpr cpp::simd<double, N> c6 = 0x1.b22004be59c73p-14;
  constexpr cpp::simd<double, N> c7 = -0x1.14bd54572305fp-18;
  constexpr cpp::simd<double, N> f = 0.25;

  cpp::simd<double, N> r2 = r * r;
  cpp::simd<double, N> r4 = r2 * r2;
  cpp::simd<double, N> p01 = cpp::multiply_add(r2, c1, c0);
  cpp::simd<double, N> p23 = cpp::multiply_add(r2, c3, c2);
  cpp::simd<double, N> p45 = cpp::multiply_add(r2, c5, c4);
  cpp::simd<double, N> p67 = cpp::multiply_add(r2, c7, c6);
  cpp::simd<double, N> p47 = cpp::multiply_add(r4, p67, p45);
  cpp::simd<double, N> p27 = cpp::multiply_add(r4, p47, p23);
  cpp::simd<double, N> p07 = cpp::multiply_add(r4, p27, p01);

  cpp::simd<double, N> factor = cpp::multiply_add(-r, r, f);

  return factor * p07;
}

// Correct specific cases which aren't able to correctly round from the normal
// codepath.
template <size_t N>
LIBC_INLINE static cpp::simd<float, N>
repair_hard_to_round(cpp::simd<float, N> x, cpp::simd<float, N> y) {
  y = (x == 0x1.fcd9eep+38f) ? 0x1.3371d2p-17f : y;
  y = (x == 0x1.3170f0p+63f) ? 0x1.fe2976p-1f : y;
  y = (x == 0x1.15313ep+69f) ? 0x1.f52484p-20f : y;

#ifndef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  y = (x == 0x1.8db252p+25f) ? -0x1.527a0ap-5f : y;
  y = (x == 0x1.aff73cp+32f) ? -0x1.d59e7ap-23f : y;
  y = (x == 0x1.47d0fep+34f) ? -0x1.149dbp-29f : y;
  y = (x == 0x1.455500p+51f) ? 0x1.115d7ep-1f : y;
  y = (x == 0x1.407c74p+100f) ? 0x1.a4a122p-19f : y;
#endif

  return y;
}

// Due to specific lane correction being expensive, we can perform a fast
// estimated check to determine if we should branch to specific hard to round
// case correction. Will return true for all hard to round cases, but can
// produce false positives. False positive rate in [0x1.90bdfap+20f, inf]:
//  With FMA: 52/901,226,755 ~= 1/2^24
//  Without FMA: 7,040,869/901,226,755 ~= 1/2^7
template <size_t N>
LIBC_INLINE static cpp::simd<bool, N>
is_maybe_hard_to_round(cpp::simd<float, N> x) {
  cpp::simd<uint32_t, N> x_bits = cpp::bit_cast<cpp::simd<uint32_t>>(x);
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  return ((x_bits * 0xd6c4de5f) & 0xefffda56) == 0x46048000;
#else
  return ((x_bits * 0x30d04f5f) & 0x8b002240) == 0x8002000;
#endif
}

template <size_t N>
LIBC_INLINE cpp::simd<float, N> cosf(cpp::simd<float, N> x) {
  using FPBits = typename fputil::FPBits<float>;
  cpp::simd<double, N> x_d = cpp::simd_cast<double>(x);
  cpp::simd<float, N> ax = cpp::abs(x);

  // If all lanes are below pi/2, we can skip the reduction entirely.
  cpp::simd<bool, N> is_small = ax <= 0x1.921fb6p+0f;
  if (cpp::all_of(is_small)) {
    constexpr cpp::simd<double, N> inv_pi = 0x1.45f306dc9c883p-2;
    return cpp::simd_cast<float>(cospif_poly(x_d * inv_pi));
  }

  // Computes the main reduction pass
  Reduction<N> reduce = fast_reduction(x_d);

  // Large inputs require a more involved reduction, as well as inf handling.
  cpp::simd<bool, N> has_large_reduction = ax > 0x1.90bdfap+20f;
  if (LIBC_UNLIKELY(cpp::any_of(has_large_reduction))) {
    cpp::simd<bool, N> is_finite = ax < FPBits::inf().get_val();
    Reduction<N> large_reduce = large_reduction(x_d);
    reduce.r = has_large_reduction ? large_reduce.r : reduce.r;
    reduce.r = is_finite ? reduce.r : FPBits::quiet_nan().get_val();
    reduce.odd = has_large_reduction ? large_reduce.odd : reduce.odd;
  }

  // Both reduction paths feed into a single polynomial evaluation + sign
  // correction.
  cpp::simd<float, N> poly = cpp::simd_cast<float>(cospif_poly(reduce.r));
  cpp::simd<uint32_t, N> sign = cpp::simd_cast<uint32_t>(reduce.odd) << 31;

  cpp::simd<float, N> y = cpp::bit_cast<cpp::simd<float>>(
      cpp::bit_cast<cpp::simd<uint32_t>>(poly) ^ sign);

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#ifndef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  y = (ax == 0x1.d2eb54p+10) ? -0x1.633eb4p-13 : y;
#endif
  if (LIBC_UNLIKELY(cpp::any_of(has_large_reduction))) {
    if (LIBC_UNLIKELY(cpp::any_of(is_maybe_hard_to_round(ax))))
      return repair_hard_to_round(ax, y);
  }
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  return y;
}

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_COSF_H
