//===-- Portable optimization macros ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This header file defines portable macros for performance optimization.

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_OPTIMIZATION_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_OPTIMIZATION_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/compiler.h" // LIBC_COMPILER_IS_CLANG

// We use a template to implement likely/unlikely to make sure that we don't
// accidentally pass an integer.
namespace LIBC_NAMESPACE_DECL {
namespace details {
template <typename T>
LIBC_INLINE constexpr bool expects_bool_condition(T value, T expected) {
  return __builtin_expect(value, expected);
}
} // namespace details
} // namespace LIBC_NAMESPACE_DECL
#define LIBC_LIKELY(x) LIBC_NAMESPACE::details::expects_bool_condition(x, true)
#define LIBC_UNLIKELY(x)                                                       \
  LIBC_NAMESPACE::details::expects_bool_condition(x, false)

#if defined(LIBC_COMPILER_IS_CLANG)
#define LIBC_LOOP_NOUNROLL _Pragma("nounroll")
#define LIBC_LOOP_UNROLL _Pragma("unroll")
#elif defined(LIBC_COMPILER_IS_GCC)
#define LIBC_LOOP_NOUNROLL _Pragma("GCC unroll 0")
#define LIBC_LOOP_UNROLL _Pragma("GCC unroll 2048")
#elif defined(LIBC_COMPILER_IS_MSVC)
#define LIBC_LOOP_NOUNROLL
#define LIBC_LOOP_UNROLL
#else
#error "Unhandled compiler"
#endif

// Defining optimization options for math functions.
// TODO: Exporting this to public generated headers?
#define LIBC_MATH_SKIP_ACCURATE_PASS 0x01
#define LIBC_MATH_SMALL_TABLES 0x02
#define LIBC_MATH_NO_ERRNO 0x04
#define LIBC_MATH_NO_EXCEPT 0x08
#define LIBC_MATH_FAST                                                         \
  (LIBC_MATH_SKIP_ACCURATE_PASS | LIBC_MATH_SMALL_TABLES |                     \
   LIBC_MATH_NO_ERRNO | LIBC_MATH_NO_EXCEPT)
#define LIBC_MATH_INTERMEDIATE_COMP_IN_FLOAT 0x10

#ifndef LIBC_MATH
#define LIBC_MATH 0
#endif // LIBC_MATH

#if (LIBC_MATH & LIBC_MATH_SKIP_ACCURATE_PASS)
#define LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#endif

#if (LIBC_MATH & LIBC_MATH_SMALL_TABLES)
#define LIBC_MATH_HAS_SMALL_TABLES
#endif

#if (LIBC_MATH & LIBC_MATH_INTERMEDIATE_COMP_IN_FLOAT)
#define LIBC_MATH_HAS_INTERMEDIATE_COMP_IN_FLOAT
#endif

#if (LIBC_MATH & LIBC_MATH_NO_ERRNO)
#define LIBC_MATH_HAS_NO_ERRNO
#endif

#if (LIBC_MATH & LIBC_MATH_NO_EXCEPT)
#define LIBC_MATH_HAS_NO_EXCEPT
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_OPTIMIZATION_H
