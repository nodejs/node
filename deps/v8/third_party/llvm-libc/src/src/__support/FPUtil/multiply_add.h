//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Common header for multiply-add implementations.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_MULTIPLY_ADD_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_MULTIPLY_ADD_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h" // LIBC_TARGET_CPU_HAS_FMA

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// Implement a simple wrapper for multiply-add operation:
//   multiply_add(x, y, z) = x*y + z
// which uses FMA instructions to speed up if available.

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(void *)), T>
multiply_add(const T &x, const T &y, const T &z) {
  T prod = x * y;
  return prod + z;
}

template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(void *)), T>
multiply_add(T x, T y, T z) {
  T prod = x * y;
  return prod + z;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#if defined(LIBC_TARGET_CPU_HAS_FMA) && !defined(LIBC_USE_CONSTEXPR)

#if !__has_builtin(__builtin_elementwise_fma) && defined(LIBC_COMPILER_IS_MSVC)
#if defined(LIBC_TARGET_ARCH_IS_X86)
#include <immintrin.h>
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include <arm64_neon.h>
#endif
#endif

// FMA instructions are available.
// We use builtins directly instead of including FMA.h to avoid a circular
// dependency: multiply_add.h -> FMA.h -> generic/FMA.h -> dyadic_float.h.
//
// TODO: for constexpr evaluation of multiply_add using FMA, we will need to
// use the generic fma implementation from generic/FMA.h.  But currently that
// implementation will use dyadic_float.h , which in turns including this
// multiply_add.h .  We will need to break the dependency to enable constexpr
// for other math functions.

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
LIBC_INLINE float multiply_add(float x, float y, float z) {
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
LIBC_INLINE double multiply_add(double x, double y, double z) {
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

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TARGET_CPU_HAS_FMA

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_MULTIPLY_ADD_H
