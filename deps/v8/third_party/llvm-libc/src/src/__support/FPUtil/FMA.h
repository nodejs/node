//===-- Common header for FMA implementations -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_FMA_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_FMA_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/generic/FMA.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h" // LIBC_TARGET_CPU_HAS_FMA

#if defined(LIBC_TARGET_CPU_HAS_FMA) &&                                        \
    !__has_builtin(__builtin_elementwise_fma) &&                               \
    defined(LIBC_COMPILER_IS_MSVC)
#if defined(LIBC_TARGET_ARCH_IS_X86)
#include <immintrin.h>
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include <arm64_neon.h>
#endif
#endif

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

template <typename OutType, typename InType>
LIBC_INLINE constexpr OutType fma(InType x, InType y, InType z) {
  return generic::fma<OutType>(x, y, z);
}

#ifdef LIBC_TARGET_CPU_HAS_FMA

#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
template <> LIBC_INLINE float fma(float x, float y, float z) {
#if __has_builtin(__builtin_elementwise_fma)
  return __builtin_elementwise_fma(x, y, z);
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
  return _mm_cvtss_f32(
      _mm_fmadd_ss(_mm_set_ss(x), _mm_set_ss(y), _mm_set_ss(z)));
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_AARCH64)
  return vget_lane_f32(vfma_f32(vdup_n_f32(z), vdup_n_f32(x), vdup_n_f32(y)),
                       0);
#else
  return __builtin_fmaf(x, y, z);
#endif
}
#endif // LIBC_TARGET_CPU_HAS_FMA_FLOAT

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
template <> LIBC_INLINE double fma(double x, double y, double z) {
#if __has_builtin(__builtin_elementwise_fma)
  return __builtin_elementwise_fma(x, y, z);
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
  return _mm_cvtsd_f64(
      _mm_fmadd_sd(_mm_set_sd(x), _mm_set_sd(y), _mm_set_sd(z)));
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_AARCH64)
  return vget_lane_f64(vfma_f64(vdup_n_f64(z), vdup_n_f64(x), vdup_n_f64(y)),
                       0);
#else
  return __builtin_fma(x, y, z);
#endif
}
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE
#endif // LIBC_TARGET_CPU_HAS_FMA

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_FMA_H
