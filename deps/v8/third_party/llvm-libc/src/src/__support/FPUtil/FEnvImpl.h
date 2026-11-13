//===-- Floating point environment manipulation functions -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_FENVIMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_FENVIMPL_H

#include "hdr/fenv_macros.h"
#include "hdr/math_macros.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h"

#ifdef LIBC_COMPILER_HAS_STDC_FENV_ACCESS
#define LIBC_FENV_ACCESS_ON _Pragma("STDC FENV_ACCESS ON")
#else
#define LIBC_FENV_ACCESS_ON
#endif

// In full build mode we are the system fenv in libc.
#if defined(LIBC_FULL_BUILD)
#undef LIBC_MATH_USE_SYSTEM_FENV
#endif // LIBC_FULL_BUILD

#if defined(LIBC_MATH_USE_SYSTEM_FENV) && !defined(LIBC_MATH_HAS_NO_EXCEPT)

// Simply call the system libc fenv.h functions, only for those that are used in
// math function implementations.
// To be used as an option for math function implementation, not to be used to
// implement fenv.h functions themselves.

#include <fenv.h>

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

LIBC_INLINE int clear_except(int excepts) {
  LIBC_FENV_ACCESS_ON
  return feclearexcept(excepts);
}

LIBC_INLINE int test_except(int excepts) {
  LIBC_FENV_ACCESS_ON
  return fetestexcept(excepts);
}

LIBC_INLINE int get_except() {
  LIBC_FENV_ACCESS_ON
  fexcept_t excepts = 0;
  fegetexceptflag(&excepts, FE_ALL_EXCEPT);
  return static_cast<int>(excepts);
}

LIBC_INLINE int set_except(int excepts) {
  LIBC_FENV_ACCESS_ON
  fexcept_t exc = static_cast<fexcept_t>(excepts);
  return fesetexceptflag(&exc, FE_ALL_EXCEPT);
}

LIBC_INLINE int raise_except(int excepts) {
  LIBC_FENV_ACCESS_ON
  return feraiseexcept(excepts);
}

LIBC_INLINE int enable_except(int) { return 0; }

LIBC_INLINE int disable_except(int) { return 0; }

LIBC_INLINE int get_round() {
  LIBC_FENV_ACCESS_ON
  return fegetround();
}

LIBC_INLINE int set_round(int rounding_mode) {
  LIBC_FENV_ACCESS_ON
  return fesetround(rounding_mode);
}

LIBC_INLINE int get_env(fenv_t *env) {
  LIBC_FENV_ACCESS_ON
  return fegetenv(env);
}

LIBC_INLINE int set_env(const fenv_t *env) {
  LIBC_FENV_ACCESS_ON
  return fesetenv(env);
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#else // !LIBC_MATH_USE_SYSTEM_FENV

#if defined(LIBC_TARGET_ARCH_IS_AARCH64) && defined(__ARM_FP)
#if defined(__APPLE__)
#include "src/__support/FPUtil/aarch64/fenv_darwin_impl.h"
#else
#include "src/__support/FPUtil/aarch64/FEnvImpl.h"
#endif

// The extra !defined(APPLE) condition is to cause x86_64 MacOS builds to use
// the dummy implementations below. Once a proper x86_64 darwin fenv is set up,
// the apple condition here should be removed.
// TODO: fully support fenv for MSVC.
#elif defined(LIBC_TARGET_ARCH_IS_X86) && !defined(__APPLE__)
#include "src/__support/FPUtil/x86_64/FEnvImpl.h"
#elif defined(LIBC_TARGET_ARCH_IS_ARM) && defined(__ARM_FP) &&                 \
    !defined(LIBC_COMPILER_IS_MSVC)
#include "src/__support/FPUtil/arm/FEnvImpl.h"
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV) && defined(__riscv_flen)
#include "src/__support/FPUtil/riscv/FEnvImpl.h"
#elif defined(LIBC_TARGET_ARCH_IS_HEXAGON)
#include "src/__support/FPUtil/hexagon/FEnvImpl.h"
#else
#define LIBC_FPUTIL_HAS_DUMMY_FENV_IMPL

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// All dummy functions silently succeed.

LIBC_INLINE int clear_except(int) { return 0; }

LIBC_INLINE int test_except(int) { return 0; }

LIBC_INLINE int get_except() { return 0; }

LIBC_INLINE int set_except(int) { return 0; }

LIBC_INLINE int raise_except(int) { return 0; }

LIBC_INLINE int enable_except(int) { return 0; }

LIBC_INLINE int disable_except(int) { return 0; }

LIBC_INLINE int get_round() { return FE_TONEAREST; }

LIBC_INLINE int set_round(int rounding_mode) {
  return (rounding_mode == FE_TONEAREST) ? 0 : 1;
}

LIBC_INLINE int get_env(fenv_t *) { return 0; }

LIBC_INLINE int set_env(const fenv_t *) { return 0; }

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL
#endif

#endif // LIBC_MATH_USE_SYSTEM_FENV

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT int
clear_except_if_required([[maybe_unused]] int excepts) {
  if (cpp::is_constant_evaluated()) {
    return 0;
  } else {
#ifndef LIBC_MATH_HAS_NO_EXCEPT
    if (math_errhandling & MATH_ERREXCEPT)
      return clear_except(excepts);
#endif // LIBC_MATH_HAS_NO_EXCEPT
    return 0;
  }
}

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT int
set_except_if_required([[maybe_unused]] int excepts) {
  if (cpp::is_constant_evaluated()) {
    return 0;
  } else {
#ifndef LIBC_MATH_HAS_NO_EXCEPT
    if (math_errhandling & MATH_ERREXCEPT)
      return set_except(excepts);
#endif // LIBC_MATH_HAS_NO_EXCEPT
    return 0;
  }
}

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT int
raise_except_if_required([[maybe_unused]] int excepts) {
  if (cpp::is_constant_evaluated()) {
    return 0;
  } else {
#ifndef LIBC_MATH_HAS_NO_EXCEPT
    if (math_errhandling & MATH_ERREXCEPT)
      return raise_except(excepts);
#endif // LIBC_MATH_HAS_NO_EXCEPT
    return 0;
  }
}

LIBC_INLINE LIBC_CONSTEXPR_DEFAULT void
set_errno_if_required([[maybe_unused]] int err) {
  if (!cpp::is_constant_evaluated()) {
#ifndef LIBC_MATH_HAS_NO_ERRNO
    if (math_errhandling & MATH_ERRNO)
      libc_errno = err;
#endif // LIBC_MATH_HAS_NO_ERRNO
  }
}

namespace internal {

template <typename T> LIBC_INLINE_VAR constexpr bool HAS_HW_FPU = false;
template <typename T> LIBC_INLINE_VAR constexpr T OVERFLOW_FACTOR = T(0);
template <typename T> LIBC_INLINE_VAR constexpr T UNDERFLOW_FACTOR = T(0);

#if defined(LIBC_TARGET_CPU_HAS_FPU_FLOAT)
template <> LIBC_INLINE_VAR constexpr bool HAS_HW_FPU<float> = true;
// 2^97 * (+/-2^97) = +/-2^194 (> max_normal = (2 - 2^-23) * 2^127).
template <> LIBC_INLINE_VAR constexpr float OVERFLOW_FACTOR<float> = 0x1.0p+97f;
// 2^-95 * (+/-2^-95) = +/-2^-190 (strictly inside (-2^-150, 2^-150)).
template <>
LIBC_INLINE_VAR constexpr float UNDERFLOW_FACTOR<float> = 0x1.0p-95f;
#endif // LIBC_TARGET_CPU_HAS_FPU_FLOAT

#if defined(LIBC_TARGET_CPU_HAS_FPU_DOUBLE)
template <> LIBC_INLINE_VAR constexpr bool HAS_HW_FPU<double> = true;
// 2^769 * (+/-2^769) = +/-2^1538 (> max_normal = (2 - 2^-52) * 2^1023).
template <>
LIBC_INLINE_VAR constexpr double OVERFLOW_FACTOR<double> = 0x1.0p+769;
// 2^-767 * (+/-2^-767) = +/-2^-1534 (strictly inside (-2^-1075, 2^-1075)).
template <>
LIBC_INLINE_VAR constexpr double UNDERFLOW_FACTOR<double> = 0x1.0p-767;
#endif // LIBC_TARGET_CPU_HAS_FPU_DOUBLE

template <typename T>
LIBC_INLINE_VAR const volatile T OVERFLOW_VAL[2] = {OVERFLOW_FACTOR<T>,
                                                    -OVERFLOW_FACTOR<T>};

template <typename T>
LIBC_INLINE_VAR const volatile T UNDERFLOW_VAL[2] = {UNDERFLOW_FACTOR<T>,
                                                     -UNDERFLOW_FACTOR<T>};

} // namespace internal

template <typename T>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT void raise_overflow_except_if_required() {
  raise_except_if_required(FE_OVERFLOW | FE_INEXACT);
}

template <typename T>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT void raise_underflow_except_if_required() {
  raise_except_if_required(FE_UNDERFLOW | FE_INEXACT);
}

#if !defined(LIBC_MATH_HAS_NO_EXCEPT) &&                                       \
    !defined(LIBC_MATH_USE_SYSTEM_FENV) &&                                     \
    !defined(LIBC_FPUTIL_HAS_DUMMY_FENV_IMPL)
#if defined(LIBC_TARGET_CPU_HAS_FPU_FLOAT)
template <>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT void
raise_overflow_except_if_required<float>() {
  if (!cpp::is_constant_evaluated()) {
    if (math_errhandling & MATH_ERREXCEPT) {
      float v = internal::OVERFLOW_VAL<float>[0];
      [[maybe_unused]] volatile float r = v * v;
    }
  }
}

template <>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT void
raise_underflow_except_if_required<float>() {
  if (!cpp::is_constant_evaluated()) {
    if (math_errhandling & MATH_ERREXCEPT) {
      float v = internal::UNDERFLOW_VAL<float>[0];
      [[maybe_unused]] volatile float r = v * v;
    }
  }
}
#endif // LIBC_TARGET_CPU_HAS_FPU_FLOAT

#if defined(LIBC_TARGET_CPU_HAS_FPU_DOUBLE)
template <>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT void
raise_overflow_except_if_required<double>() {
  if (!cpp::is_constant_evaluated()) {
    if (math_errhandling & MATH_ERREXCEPT) {
      double v = internal::OVERFLOW_VAL<double>[0];
      [[maybe_unused]] volatile double r = v * v;
    }
  }
}

template <>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT void
raise_underflow_except_if_required<double>() {
  if (!cpp::is_constant_evaluated()) {
    if (math_errhandling & MATH_ERREXCEPT) {
      double v = internal::UNDERFLOW_VAL<double>[0];
      [[maybe_unused]] volatile double r = v * v;
    }
  }
}
#endif // LIBC_TARGET_CPU_HAS_FPU_DOUBLE
#endif // !LIBC_MATH_HAS_NO_EXCEPT && !LIBC_MATH_USE_SYSTEM_FENV &&
       // !LIBC_FPUTIL_HAS_DUMMY_FENV_IMPL

#undef LIBC_FPUTIL_HAS_DUMMY_FENV_IMPL

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_FENVIMPL_H
