//===-- x86_64 floating point env manipulation functions --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENVIMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENVIMPL_H

#include "hdr/fenv_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/types.h"

#if !defined(LIBC_TARGET_ARCH_IS_X86)
#error "Invalid include"
#endif

#if (defined(__i386__) && !defined(__SSE__)) ||                                \
    (defined(_M_IX86_FP) && (_M_IX86_FP == 0))
// When SSE is not available, we will only touch x87 floating point environment.
#include "src/__support/FPUtil/x86_64/fenv_x87_only.h"
#else // __SSE__

#ifndef LIBC_COMPILER_IS_MSVC
#include "src/__support/FPUtil/x86_64/fenv_x87_utils.h"
#endif // !LIBC_COMPILER_IS_MSVC

#include "src/__support/FPUtil/x86_64/fenv_mxcsr_utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

LIBC_INLINE static int clear_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  sse::clear_except(x86_excepts);

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  x87::clear_except(x86_excepts);
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

  return 0;
}

LIBC_INLINE static int test_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  uint16_t tested_excepts = sse::test_except(x86_excepts);

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  tested_excepts |= x87::test_except(x86_excepts);
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

  return internal::get_macro_from_exception_status(tested_excepts);
}

LIBC_INLINE static int get_except() {
  uint16_t excepts = sse::get_except();

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  excepts |= x87::get_except();
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

  return internal::get_macro_from_exception_status(excepts);
}

LIBC_INLINE static int set_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  sse::set_except(x86_excepts);

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  x87::set_except(x86_excepts);
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

  return 0;
}

// We will only OR sse exception flags.  Even though this might make x87 and
// sse exception flags not in sync, the results will be synchronized when
// reading with get_except or test_except.
LIBC_INLINE static int raise_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  sse::raise_except(x86_excepts);
  return 0;
}

LIBC_INLINE static int enable_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  uint16_t old_excepts = sse::enable_except(x86_excepts);

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  old_excepts |= x87::enable_except(x86_excepts);
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

  return internal::get_macro_from_exception_status(old_excepts);
}

LIBC_INLINE static int disable_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  uint16_t old_excepts = sse::disable_except(x86_excepts);

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  old_excepts |= x87::disable_except(x86_excepts);
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

  return internal::get_macro_from_exception_status(old_excepts);
}

LIBC_INLINE static int get_round() {
  uint16_t rounding_mode = sse::get_round();
  return internal::get_macro_from_rounding_control(rounding_mode);
}

LIBC_INLINE static int set_round(int rounding_mode) {
  uint16_t rounding = internal::get_rounding_control_from_macro(rounding_mode);
  if (LIBC_UNLIKELY(rounding == internal::RoundingControl::RC_ERROR))
    return -1;
  sse::set_round(rounding);

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  x87::set_round(rounding);
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

  return 0;
}

LIBC_INLINE static int get_env(fenv_t *env) {
#ifndef LIBC_COMPILER_IS_MSVC
  if constexpr (sizeof(fenv_t) >= sizeof(internal::X87StateDescriptor)) {
    // The fenv_t is expected to have x87 floating point environment.
    internal::X87StateDescriptor x87_state;
    x87::get_x87_state_descriptor(x87_state);
    uint32_t mxcsr = static_cast<uint32_t>(sse::get_mxcsr());
    if constexpr (sizeof(fenv_t) == sizeof(internal::X87StateDescriptor)) {
      // The fenv_t is expected to have only x87 floating point environment, so
      // we merge sse data to x87 state.
      internal::mxcsr_to_x87_state(static_cast<uint16_t>(mxcsr), x87_state);
      // Copy the state data;
      const char *x87_state_ptr = reinterpret_cast<const char *>(&x87_state);
      char *fenv_ptr = reinterpret_cast<char *>(env);
      cpp::inline_copy<sizeof(x87_state)>(x87_state_ptr, fenv_ptr);
    } else {
      // We expect to have at least extra 32-bit for mxcsr register in the
      // fenv_t.
      static_assert(
          sizeof(sizeof(fenv_t) >=
                 sizeof(internal::X87StateDescriptor) + sizeof(uint32_t)));
      const char *x87_state_ptr = reinterpret_cast<const char *>(&x87_state);
      const char *mxcsr_ptr = reinterpret_cast<const char *>(&mxcsr);
      char *fenv_ptr = reinterpret_cast<char *>(env);
      cpp::inline_copy<sizeof(x87_state)>(x87_state_ptr, fenv_ptr);
      cpp::inline_copy<sizeof(mxcsr)>(mxcsr_ptr, fenv_ptr + sizeof(x87_state));
    }
  } else
#endif // LIBC_COMPILER_IS_MSVC
    if constexpr (sizeof(fenv_t) == 2 * sizeof(uint32_t)) {
      // fenv_t has 2 * uint32_t to store mxcsr with control + status
      // separately. We will just duplicate mxcsr on those two fields.
      uint32_t mxcsr = static_cast<uint32_t>(sse::get_mxcsr());
      const char *mxcsr_ptr = reinterpret_cast<const char *>(&mxcsr);
      char *fenv_ptr = reinterpret_cast<char *>(env);
      cpp::inline_copy<sizeof(mxcsr)>(mxcsr_ptr, fenv_ptr);
      cpp::inline_copy<sizeof(mxcsr)>(mxcsr_ptr, fenv_ptr + sizeof(mxcsr));
    } else {
      // Just copy mxcsr over to fenv_t.
      // Make sure fenv_t is big enough.
      static_assert(sizeof(fenv_t) >= sizeof(uint32_t));
      uint32_t mxcsr = static_cast<uint32_t>(sse::get_mxcsr());
      const char *mxcsr_ptr = reinterpret_cast<const char *>(&mxcsr);
      char *fenv_ptr = reinterpret_cast<char *>(env);
      cpp::inline_copy<sizeof(mxcsr)>(mxcsr_ptr, fenv_ptr);
    }
  return 0;
}

LIBC_INLINE static int set_env(const fenv_t *env) {
  if (env == FE_DFL_ENV) {
#ifndef LIBC_COMPILER_IS_MSVC
    x87::initialize_x87_state();
#endif // LIBC_COMPILER_IS_MSVC
    // Initial state of mxcsr:
    // Round-to-nearest, all exceptions are masked, all exception flags are
    // cleared.
    sse::write_mxcsr(0x1f80);
    return 0;
  }

#ifndef LIBC_COMPILER_IS_MSVC
  if constexpr (sizeof(fenv_t) > sizeof(internal::X87StateDescriptor)) {
    // We expect to have at least extra 32-bit for mxcsr register in the fenv_t.
    static_assert(
        sizeof(sizeof(fenv_t) >=
               sizeof(internal::X87StateDescriptor) + sizeof(uint32_t)));
    internal::X87StateDescriptor x87_state;
    uint32_t mxcsr = 0;

    char *x87_state_ptr = reinterpret_cast<char *>(&x87_state);
    char *mxcsr_ptr = reinterpret_cast<char *>(&mxcsr);
    const char *fenv_ptr = reinterpret_cast<const char *>(env);

    cpp::inline_copy<sizeof(x87_state)>(fenv_ptr, x87_state_ptr);
    cpp::inline_copy<sizeof(mxcsr)>(fenv_ptr + sizeof(x87_state), mxcsr_ptr);

    x87::write_x87_state_descriptor(x87_state);
    sse::write_mxcsr(mxcsr);
  } else if constexpr (sizeof(fenv_t) == sizeof(internal::X87StateDescriptor)) {
    const internal::X87StateDescriptor *x87_state_ptr =
        reinterpret_cast<const internal::X87StateDescriptor *>(env);
    uint32_t mxcsr = internal::x87_state_to_mxcsr(*x87_state_ptr);

    x87::write_x87_state_descriptor(*x87_state_ptr);
    sse::write_mxcsr(mxcsr);
  } else
#endif // LIBC_COMPILER_IS_MSVC
    if constexpr (sizeof(fenv_t) == 2 * sizeof(uint32_t)) {
      // fenv_t has 2 * uint32_t to store mxcsr with control + status
      // separately. We will just merge mxcsr on those two fields.
      uint32_t mxcsr = 0, mxcsr_hi = 0;
      char *mxcsr_ptr = reinterpret_cast<char *>(&mxcsr);
      char *mxcsr_hi_ptr = reinterpret_cast<char *>(&mxcsr_hi);
      const char *fenv_ptr = reinterpret_cast<const char *>(env);
      cpp::inline_copy<sizeof(mxcsr)>(fenv_ptr, mxcsr_ptr);
      cpp::inline_copy<sizeof(mxcsr_hi)>(fenv_ptr + sizeof(mxcsr),
                                         mxcsr_hi_ptr);
      sse::write_mxcsr(mxcsr | mxcsr_hi);
    } else {
      // Just copy mxcsr over to fenv_t.
      // Make sure fenv_t is big enough.
      static_assert(sizeof(fenv_t) >= sizeof(uint32_t));
      uint32_t mxcsr = 0;
      char *mxcsr_ptr = reinterpret_cast<char *>(&mxcsr);
      const char *fenv_ptr = reinterpret_cast<const char *>(env);
      cpp::inline_copy<sizeof(mxcsr)>(fenv_ptr, mxcsr_ptr);
      sse::write_mxcsr(mxcsr);
    }

  return 0;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // __SSE__

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENVIMPL_H
