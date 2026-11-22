//===-- Square root of IEEE 754 floating point numbers ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_SQRT_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_SQRT_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h"

#if !defined(LIBC_TARGET_ARCH_IS_AARCH64)
#error "Invalid include"
#endif

#include "src/__support/FPUtil/generic/sqrt.h"

#ifdef LIBC_COMPILER_IS_MSVC
#include <arm64_neon.h>
#endif

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

#ifdef LIBC_TARGET_CPU_HAS_FPU_FLOAT
template <> LIBC_INLINE float sqrt<float>(float x) {
#ifdef LIBC_COMPILER_IS_MSVC
  return vget_lane_f32(vsqrt_f32(vdup_n_f32(x)), 0);
#else
  float y;
  asm("fsqrt %s0, %s1\n\t" : "=w"(y) : "w"(x));
  return y;
#endif
}
#endif // LIBC_TARGET_CPU_HAS_FPU_FLOAT

#ifdef LIBC_TARGET_CPU_HAS_FPU_DOUBLE
template <> LIBC_INLINE double sqrt<double>(double x) {
#ifdef LIBC_COMPILER_IS_MSVC
  return vget_lane_f64(vsqrt_f64(vdup_n_f64(x)), 0);
#else
  double y;
  asm("fsqrt %d0, %d1\n\t" : "=w"(y) : "w"(x));
  return y;
#endif
}
#endif // LIBC_TARGET_CPU_HAS_FPU_DOUBLE

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_SQRT_H
