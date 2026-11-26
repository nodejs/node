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
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h" // LIBC_COMPILER_IS_CLANG
#include "src/__support/macros/properties/cpu_features.h"

// We use a template to implement likely/unlikely to make sure that we don't
// accidentally pass an integer.
// Note that `__builtin_expect` requires the second argument to be a constant
// expression.
namespace LIBC_NAMESPACE_DECL {
namespace details {
template <typename T>
LIBC_ALWAYS_INLINE constexpr bool expects_bool_condition(T value) = delete;

template <>
LIBC_ALWAYS_INLINE constexpr bool expects_bool_condition<bool>(bool value) {
  return value;
}

template <typename T> void do_not_optimize_impl(T &) = delete;

// Zero-cost SSA value barrier for float and double: keeps `val` in its native
// register class without spilling to memory or moving across GPR/FPR banks,
// while preventing the optimizer from looking through or duplicating `val`.
// Kept in non-constexpr helpers because C++17 rejects inline `asm` statements
// directly inside `constexpr` functions (`-Wc++20-extensions`). Skipped on
// `cl.exe` (`LIBC_COMPILER_IS_MSVC`, which excludes `clang-cl`) because MSVC
// does not support GNU extended `asm` register constraints.
LIBC_ALWAYS_INLINE void do_not_optimize_impl(LIBC_UNUSED float &val) {
#if !defined(LIBC_COMPILER_IS_MSVC)
#if defined(LIBC_TARGET_CPU_HAS_ARM_FPU_FLOAT)
  asm("" : "+w"(val));
#elif defined(LIBC_TARGET_ARCH_IS_X86) && defined(LIBC_TARGET_CPU_HAS_SSE2)
  asm("" : "+x"(val));
#elif defined(LIBC_TARGET_ARCH_IS_X86)
  asm("" : "+t"(val));
#elif defined(LIBC_TARGET_CPU_HAS_RISCV_FPU_FLOAT)
  asm("" : "+f"(val));
#elif defined(LIBC_TARGET_ARCH_IS_AMDGPU)
  asm("" : "+v"(val));
#elif defined(LIBC_TARGET_ARCH_IS_NVPTX)
  asm("" : "+f"(val));
#elif defined(LIBC_TARGET_ARCH_IS_ARM) ||                                      \
    defined(LIBC_TARGET_ARCH_IS_ANY_RISCV) ||                                  \
    defined(LIBC_TARGET_ARCH_IS_WASM) || defined(LIBC_TARGET_ARCH_IS_HEXAGON)
  asm("" : "+r"(val));
#endif
#endif // !LIBC_COMPILER_IS_MSVC
}

LIBC_ALWAYS_INLINE void do_not_optimize_impl(LIBC_UNUSED double &val) {
#if !defined(LIBC_COMPILER_IS_MSVC)
#if defined(LIBC_TARGET_CPU_HAS_ARM_FPU_DOUBLE)
  asm("" : "+w"(val));
#elif defined(LIBC_TARGET_ARCH_IS_X86) && defined(LIBC_TARGET_CPU_HAS_SSE2)
  asm("" : "+x"(val));
#elif defined(LIBC_TARGET_ARCH_IS_X86)
  asm("" : "+t"(val));
#elif defined(LIBC_TARGET_CPU_HAS_RISCV_FPU_DOUBLE)
  asm("" : "+f"(val));
#elif defined(LIBC_TARGET_ARCH_IS_AMDGPU)
  asm("" : "+v"(val));
#elif defined(LIBC_TARGET_ARCH_IS_NVPTX)
  asm("" : "+d"(val));
#elif defined(LIBC_TARGET_ARCH_IS_RISCV64) || defined(LIBC_TARGET_ARCH_IS_WASM)
  asm("" : "+r"(val));
#endif
#endif // !LIBC_COMPILER_IS_MSVC
}

template <typename T>
LIBC_ALWAYS_INLINE LIBC_CONSTEXPR_DEFAULT void do_not_optimize(T &val) {
#if LIBC_HAS_BUILTIN_IS_CONSTANT_EVALUATED
  if (!__builtin_is_constant_evaluated())
#endif
    do_not_optimize_impl(val);
}

} // namespace details
} // namespace LIBC_NAMESPACE_DECL

#define LIBC_DO_NOT_OPTIMIZE(var) LIBC_NAMESPACE::details::do_not_optimize(var)

#define LIBC_LIKELY(x)                                                         \
  (__builtin_expect(LIBC_NAMESPACE::details::expects_bool_condition((x)), true))
#define LIBC_UNLIKELY(x)                                                       \
  (__builtin_expect(LIBC_NAMESPACE::details::expects_bool_condition((x)),      \
                    false))

#if defined(LIBC_COMPILER_IS_CLANG)
#define LIBC_LOOP_NOUNROLL _Pragma("nounroll")
#define LIBC_LOOP_UNROLL _Pragma("unroll")
#elif defined(LIBC_COMPILER_IS_GCC)
#if LIBC_COMPILER_GCC_VER >= 800
#define LIBC_LOOP_NOUNROLL _Pragma("GCC unroll 0")
#define LIBC_LOOP_UNROLL _Pragma("GCC unroll 2048")
#else
#define LIBC_LOOP_NOUNROLL
#define LIBC_LOOP_UNROLL
#endif
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
#define LIBC_MATH_INTERMEDIATE_COMP_IN_FLOAT 0x10
#define LIBC_MATH_ASSUME_ROUND_NEAREST_ONLY 0x20
#define LIBC_MATH_FAST                                                         \
  (LIBC_MATH_SKIP_ACCURATE_PASS | LIBC_MATH_SMALL_TABLES |                     \
   LIBC_MATH_NO_ERRNO | LIBC_MATH_NO_EXCEPT |                                  \
   LIBC_MATH_ASSUME_ROUND_NEAREST_ONLY)

#ifndef LIBC_MATH
#define LIBC_MATH 0
#endif // LIBC_MATH

#if ((LIBC_MATH) & LIBC_MATH_SKIP_ACCURATE_PASS)
#define LIBC_MATH_HAS_SKIP_ACCURATE_PASS
#endif

#if ((LIBC_MATH) & LIBC_MATH_SMALL_TABLES)
#define LIBC_MATH_HAS_SMALL_TABLES
#define LIBC_MATH_SMALL_TABLES_INLINE LIBC_NOINLINE LIBC_INLINE
#else
#define LIBC_MATH_SMALL_TABLES_INLINE LIBC_ALWAYS_INLINE
#endif

#if ((LIBC_MATH) & LIBC_MATH_INTERMEDIATE_COMP_IN_FLOAT)
#define LIBC_MATH_HAS_INTERMEDIATE_COMP_IN_FLOAT
#endif

#if ((LIBC_MATH) & LIBC_MATH_NO_ERRNO)
#define LIBC_MATH_HAS_NO_ERRNO
#endif

#if ((LIBC_MATH) & LIBC_MATH_NO_EXCEPT)
#define LIBC_MATH_HAS_NO_EXCEPT
#endif

#if ((LIBC_MATH) & LIBC_MATH_ASSUME_ROUND_NEAREST_ONLY)
#define LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
#define LIBC_DO_NOT_OPTIMIZE_ROUNDING(var) ((void)(var))
#else
#define LIBC_DO_NOT_OPTIMIZE_ROUNDING(var) LIBC_DO_NOT_OPTIMIZE(var)
#endif

// Helper macros to compare math implementation namespace tokens in #if
// directives (e.g. when LIBC_MATH_<FUNC>_IMPL is overridden via -D).
#define LIBC_MATH_IMPL_ID_double_eval 1
#define LIBC_MATH_IMPL_ID_float_eval 2
#define LIBC_MATH_IMPL_ID_float_eval_cr_rn 3
#define LIBC_MATH_IMPL_ID_integer_eval 4
#define LIBC_MATH_IMPL_CONCAT_INNER(a, b) a##b
#define LIBC_MATH_IMPL_CONCAT(a, b) LIBC_MATH_IMPL_CONCAT_INNER(a, b)
#define LIBC_MATH_IMPL_IS(impl, target)                                        \
  (LIBC_MATH_IMPL_CONCAT(LIBC_MATH_IMPL_ID_, impl) ==                          \
   LIBC_MATH_IMPL_CONCAT(LIBC_MATH_IMPL_ID_, target))

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_OPTIMIZATION_H
