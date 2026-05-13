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

// In full build mode we are the system fenv in libc.
#if defined(LIBC_FULL_BUILD)
#undef LIBC_MATH_USE_SYSTEM_FENV
#endif // LIBC_FULL_BUILD

#if defined(LIBC_MATH_USE_SYSTEM_FENV)

// Simply call the system libc fenv.h functions, only for those that are used in
// math function implementations.
// To be used as an option for math function implementation, not to be used to
// implement fenv.h functions themselves.

#include <fenv.h>

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

LIBC_INLINE int clear_except(int excepts) { return feclearexcept(excepts); }

LIBC_INLINE int test_except(int excepts) { return fetestexcept(excepts); }

LIBC_INLINE int get_except() {
  fexcept_t excepts = 0;
  fegetexceptflag(&excepts, FE_ALL_EXCEPT);
  return static_cast<int>(excepts);
}

LIBC_INLINE int set_except(int excepts) {
  fexcept_t exc = static_cast<fexcept_t>(excepts);
  return fesetexceptflag(&exc, FE_ALL_EXCEPT);
}

LIBC_INLINE int raise_except(int excepts) { return feraiseexcept(excepts); }

LIBC_INLINE int get_round() { return fegetround(); }

LIBC_INLINE int set_round(int rounding_mode) {
  return fesetround(rounding_mode);
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#else // !LIBC_MATH_USE_SYSTEM_FENV

#if defined(LIBC_TARGET_ARCH_IS_AARCH64) && defined(__ARM_FP)
#if defined(__APPLE__)
#include "aarch64/fenv_darwin_impl.h"
#else
#include "aarch64/FEnvImpl.h"
#endif

// The extra !defined(APPLE) condition is to cause x86_64 MacOS builds to use
// the dummy implementations below. Once a proper x86_64 darwin fenv is set up,
// the apple condition here should be removed.
// TODO: fully support fenv for MSVC.
#elif defined(LIBC_TARGET_ARCH_IS_X86) && !defined(__APPLE__)
#include "x86_64/FEnvImpl.h"
#elif defined(LIBC_TARGET_ARCH_IS_ARM) && defined(__ARM_FP) &&                 \
    !defined(LIBC_COMPILER_IS_MSVC)
#include "arm/FEnvImpl.h"
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV) && defined(__riscv_flen)
#include "riscv/FEnvImpl.h"
#else

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

LIBC_INLINE static constexpr int
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

LIBC_INLINE static constexpr int
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

LIBC_INLINE static constexpr int
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

LIBC_INLINE static constexpr void
set_errno_if_required([[maybe_unused]] int err) {
  if (!cpp::is_constant_evaluated()) {
#ifndef LIBC_MATH_HAS_NO_ERRNO
    if (math_errhandling & MATH_ERRNO)
      libc_errno = err;
#endif // LIBC_MATH_HAS_NO_ERRNO
  }
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_FENVIMPL_H
