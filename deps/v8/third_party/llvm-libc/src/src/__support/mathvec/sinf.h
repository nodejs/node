//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implemenation for single-precision SIMD sin.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_SINF_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_SINF_H

#include "src/__support/CPP/bit.h"
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
LIBC_INLINE static cpp::simd<double, N> sinpif_poly(cpp::simd<double, N> r) {
  // Approximate sin(pi * r) for |r| <= 0.5.
  // These coefficients aren't produced directly via sollya, but rather
  // are fine-tuned by iterative adjustment to remove hard to round cases.
  // TODO: Create a tool to deterministically reproduce these coefficients.
  // see https://github.com/llvm/llvm-project/issues/220984
  constexpr cpp::simd<double, N> c0 = 0x1.921fb54442d15p1;
  constexpr cpp::simd<double, N> c1 = -0x1.4abbce625bcbdp2;
  constexpr cpp::simd<double, N> c2 = 0x1.466bc67749fe8p1;
  constexpr cpp::simd<double, N> c3 = -0x1.32d2ccde52aaap-1;
  constexpr cpp::simd<double, N> c4 = 0x1.5078311af79bdp-4;
  constexpr cpp::simd<double, N> c5 = -0x1.e305b6b7642fbp-8;
  constexpr cpp::simd<double, N> c6 = 0x1.e889fb0d0db0dp-12;
  constexpr cpp::simd<double, N> c7 = -0x1.611a523a97eb1p-16;

  cpp::simd<double, N> r2 = r * r;
  cpp::simd<double, N> r4 = r2 * r2;
  cpp::simd<double, N> p01 = cpp::multiply_add(r2, c1, c0);
  cpp::simd<double, N> p23 = cpp::multiply_add(r2, c3, c2);
  cpp::simd<double, N> p45 = cpp::multiply_add(r2, c5, c4);
  cpp::simd<double, N> p67 = cpp::multiply_add(r2, c7, c6);
  cpp::simd<double, N> p47 = cpp::multiply_add(r4, p67, p45);
  cpp::simd<double, N> p27 = cpp::multiply_add(r4, p47, p23);
  cpp::simd<double, N> p07 = cpp::multiply_add(r4, p27, p01);
  return r * p07;
}

template <size_t N>
LIBC_INLINE cpp::simd<float, N> sinf(cpp::simd<float, N> x) {
  using FPBits = typename fputil::FPBits<float>;

  cpp::simd<float, N> ax = cpp::abs(x);
  cpp::simd<uint32_t, N> x_sign = cpp::bit_cast<cpp::simd<uint32_t>>(x) ^
                                  cpp::bit_cast<cpp::simd<uint32_t>>(ax);
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  constexpr float large_reduction_bound = 0x1.6d28ce103p+52f;
#else
  constexpr float large_reduction_bound = 0x1.0p+51f;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  cpp::simd<bool, N> has_large_reduction = (ax > large_reduction_bound);

  cpp::simd<double, N> x_d = cpp::simd_cast<double>(ax);
  Reduction<N> reduce{};

  // Values at or below the large bound can use a fast Cody-Waite reduction.
  if (LIBC_LIKELY(!cpp::all_of(has_large_reduction)))
    reduce = fast_reduction(x_d);

  // Large inputs require a more involved reduction, as well as inf handling.
  if (LIBC_UNLIKELY(cpp::any_of(has_large_reduction))) {
    cpp::simd<bool, N> is_finite = ax < FPBits::inf().get_val();
    Reduction<N> large_reduce = large_reduction(x_d);
    reduce.r = has_large_reduction ? large_reduce.r : reduce.r;
    reduce.odd = has_large_reduction ? large_reduce.odd : reduce.odd;
    reduce.r = is_finite ? reduce.r : FPBits::quiet_nan().get_val();
  }

  // Both reduction paths feed into a single polynomial evaluation + sign
  // correction.
  cpp::simd<float, N> poly = cpp::simd_cast<float>(sinpif_poly(reduce.r));
  cpp::simd<uint32_t, N> sign = cpp::simd_cast<uint32_t>(reduce.odd) << 31;

  // XORing the sign correction with the input sign preserves -0.
  sign ^= x_sign;
  return cpp::bit_cast<cpp::simd<float>>(
      cpp::bit_cast<cpp::simd<uint32_t>>(poly) ^ sign);
}
} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_SINF_H
