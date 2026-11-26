//===--- Round floating point to nearest integer on aarch64 -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_NEAREST_INTEGER_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_NEAREST_INTEGER_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"

#if !defined(LIBC_TARGET_ARCH_IS_AARCH64)
#error "Invalid include"
#endif

#ifdef LIBC_COMPILER_IS_MSVC
#include <arm64_neon.h>
#endif

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace internal {

// These wrappers are kept out of the constexpr functions below because inline
// assembly is not allowed in a constexpr function before C++20.
LIBC_INLINE float hardware_nearest_integer(float x) {
#ifdef LIBC_COMPILER_IS_MSVC
  return vget_lane_f32(vrndn_f32(vdup_n_f32(x)), 0);
#else
  float result;
  __asm__ __volatile__("frintn %s0, %s1\n\t" : "=w"(result) : "w"(x));
  return result;
#endif
}

LIBC_INLINE double hardware_nearest_integer(double x) {
#ifdef LIBC_COMPILER_IS_MSVC
  return vget_lane_f64(vrndn_f64(vdup_n_f64(x)), 0);
#else
  double result;
  __asm__ __volatile__("frintn %d0, %d1\n\t" : "=w"(result) : "w"(x));
  return result;
#endif
}

} // namespace internal

// generic::nearest_integer is defined in
// "src/__support/FPUtil/nearest_integer.h", the only header that includes this
// file.
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT float nearest_integer(float x) {
  if (cpp::is_constant_evaluated()) {
    return generic::nearest_integer(x);
  } else {
    return internal::hardware_nearest_integer(x);
  }
}

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT double nearest_integer(double x) {
  if (cpp::is_constant_evaluated()) {
    return generic::nearest_integer(x);
  } else {
    return internal::hardware_nearest_integer(x);
  }
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_NEAREST_INTEGER_H
