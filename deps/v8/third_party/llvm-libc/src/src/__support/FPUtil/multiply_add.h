//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Common header for multiply-add and multiply-sub implementations.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_MULTIPLY_ADD_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_MULTIPLY_ADD_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h" // LIBC_TARGET_CPU_HAS_FMA

#if defined(LIBC_TARGET_CPU_HAS_FMA)
#if !__has_builtin(__builtin_elementwise_fma) && defined(LIBC_COMPILER_IS_MSVC)
#if defined(LIBC_TARGET_ARCH_IS_X86)
#include <immintrin.h>
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include <arm64_neon.h>
#endif
#endif
#endif // LIBC_TARGET_CPU_HAS_FMA

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace internal {

template <typename T> struct HasHardwareFma : cpp::false_type {};

template <typename T> T fma_impl(T, T, T) = delete;
template <typename T> T fms_impl(T, T, T) = delete;
template <typename T> T fnma_impl(T, T, T) = delete;

// Hardware FMA helpers use builtins directly instead of including FMA.h to
// avoid a circular dependency:
//   multiply_add.h -> FMA.h -> generic/FMA.h -> dyadic_float.h.
#if defined(LIBC_TARGET_CPU_HAS_FMA)

#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
template <> struct HasHardwareFma<float> : cpp::true_type {};

LIBC_ALWAYS_INLINE float fma_impl(float x, float y, float z) {
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

LIBC_ALWAYS_INLINE float fms_impl(float x, float y, float z) {
#if __has_builtin(__builtin_elementwise_fma)
  return __builtin_elementwise_fma(x, y, -z);
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
  return _mm_cvtss_f32(
      _mm_fmsub_ss(_mm_set_ss(x), _mm_set_ss(y), _mm_set_ss(z)));
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_AARCH64)
  return vget_lane_f32(vfma_f32(vdup_n_f32(-z), vdup_n_f32(x), vdup_n_f32(y)),
                       0);
#else
  return __builtin_fmaf(x, y, -z);
#endif
}

LIBC_ALWAYS_INLINE float fnma_impl(float x, float y, float z) {
#if __has_builtin(__builtin_elementwise_fma)
  return __builtin_elementwise_fma(-x, y, z);
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
  return _mm_cvtss_f32(
      _mm_fnmadd_ss(_mm_set_ss(x), _mm_set_ss(y), _mm_set_ss(z)));
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_AARCH64)
  return vget_lane_f32(vfms_f32(vdup_n_f32(z), vdup_n_f32(x), vdup_n_f32(y)),
                       0);
#else
  return __builtin_fmaf(-x, y, z);
#endif
}
#endif // LIBC_TARGET_CPU_HAS_FMA_FLOAT

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
template <> struct HasHardwareFma<double> : cpp::true_type {};

LIBC_ALWAYS_INLINE double fma_impl(double x, double y, double z) {
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

LIBC_ALWAYS_INLINE double fms_impl(double x, double y, double z) {
#if __has_builtin(__builtin_elementwise_fma)
  return __builtin_elementwise_fma(x, y, -z);
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
  return _mm_cvtsd_f64(
      _mm_fmsub_sd(_mm_set_sd(x), _mm_set_sd(y), _mm_set_sd(z)));
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_AARCH64)
  return vget_lane_f64(vfma_f64(vdup_n_f64(-z), vdup_n_f64(x), vdup_n_f64(y)),
                       0);
#else
  return __builtin_fma(x, y, -z);
#endif
}

LIBC_ALWAYS_INLINE double fnma_impl(double x, double y, double z) {
#if __has_builtin(__builtin_elementwise_fma)
  return __builtin_elementwise_fma(-x, y, z);
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
  return _mm_cvtsd_f64(
      _mm_fnmadd_sd(_mm_set_sd(x), _mm_set_sd(y), _mm_set_sd(z)));
#elif defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_AARCH64)
  return vget_lane_f64(vfms_f64(vdup_n_f64(z), vdup_n_f64(x), vdup_n_f64(y)),
                       0);
#else
  return __builtin_fma(-x, y, z);
#endif
}
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

#endif // LIBC_TARGET_CPU_HAS_FMA

} // namespace internal

// Compute x * y + z, using hardware FMA when available.
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
multiply_add(const T &x, const T &y, const T &z) {
  T prod = x * y;
  return prod + z;
}

template <typename T>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
multiply_add(T x, T y, T z) {
  if constexpr (internal::HasHardwareFma<T>::value) {
    if (!cpp::is_constant_evaluated())
      return internal::fma_impl(x, y, z);
  }
  T prod = x * y;
  return prod + z;
}

// Compute x * y - z, using hardware FMA when available.
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
multiply_sub(const T &x, const T &y, const T &z) {
  T prod = x * y;
  return prod - z;
}

template <typename T>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
multiply_sub(T x, T y, T z) {
  if constexpr (internal::HasHardwareFma<T>::value) {
    if (!cpp::is_constant_evaluated())
      return internal::fms_impl(x, y, z);
  }
  T prod = x * y;
  return prod - z;
}

// Compute x - y * z, using hardware FMA when available.
// Without FMA, when directed rounding modes (FE_UPWARD / FE_DOWNWARD) may be
// active, evaluate x + ((-y) * z) instead of x - (y * z) for floating-point
// types so that (-y) * z is rounded in the same direction as the outer sum
// when y or z is constant (e.g., for x = min_denormal and 0 < z < 1 in
// FE_UPWARD, RU(x + RU((-x) * z)) = x, whereas RU(x - RU(x * z)) = 0).
template <typename T>
LIBC_INLINE constexpr cpp::enable_if_t<(sizeof(T) > sizeof(double)), T>
sub_multiply(const T &x, const T &y, const T &z) {
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  if constexpr (cpp::is_floating_point_v<T>) {
    T prod = (-y) * z;
    return x + prod;
  }
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  T prod = y * z;
  return x - prod;
}

template <typename T>
LIBC_ALWAYS_INLINE constexpr cpp::enable_if_t<(sizeof(T) <= sizeof(double)), T>
sub_multiply(T x, T y, T z) {
  if constexpr (internal::HasHardwareFma<T>::value) {
    if (!cpp::is_constant_evaluated())
      return internal::fnma_impl(y, z, x);
  }
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  if constexpr (cpp::is_floating_point_v<T>) {
    T prod = (-y) * z;
    return x + prod;
  }
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  T prod = y * z;
  return x - prod;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_MULTIPLY_ADD_H
