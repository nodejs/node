//===-- Square root of IEEE 754 floating point numbers ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_RISCV_SQRT_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_RISCV_SQRT_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/cpu_features.h"

#if !defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#error "Invalid include"
#endif

#include "src/__support/FPUtil/generic/sqrt.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace internal {

// These wrappers are kept out of the constexpr specializations below because
// inline assembly is not allowed in a constexpr function before C++20.
#ifdef LIBC_TARGET_CPU_HAS_FPU_FLOAT
LIBC_INLINE float hardware_sqrt(float x) {
  float result;
  asm("fsqrt.s %0, %1\n\t" : "=f"(result) : "f"(x));
  return result;
}
#endif // LIBC_TARGET_CPU_HAS_FPU_FLOAT

#ifdef LIBC_TARGET_CPU_HAS_FPU_DOUBLE
LIBC_INLINE double hardware_sqrt(double x) {
  double result;
  asm("fsqrt.d %0, %1\n\t" : "=f"(result) : "f"(x));
  return result;
}
#endif // LIBC_TARGET_CPU_HAS_FPU_DOUBLE

} // namespace internal

#ifdef LIBC_TARGET_CPU_HAS_FPU_FLOAT
template <> LIBC_INLINE LIBC_CONSTEXPR_DEFAULT float sqrt<float>(float x) {
  if (cpp::is_constant_evaluated()) {
    return generic::sqrt<float>(x);
  } else {
    return internal::hardware_sqrt(x);
  }
}
#endif // LIBC_TARGET_CPU_HAS_FPU_FLOAT

#ifdef LIBC_TARGET_CPU_HAS_FPU_DOUBLE
template <> LIBC_INLINE LIBC_CONSTEXPR_DEFAULT double sqrt<double>(double x) {
  if (cpp::is_constant_evaluated()) {
    return generic::sqrt<double>(x);
  } else {
    return internal::hardware_sqrt(x);
  }
}
#endif // LIBC_TARGET_CPU_HAS_FPU_DOUBLE

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_RISCV_SQRT_H
