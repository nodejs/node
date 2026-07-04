//===-- x87 floating point env manipulation functions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X87_ONLY_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X87_ONLY_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/x86_64/fenv_x87_utils.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/sanitizer.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

using internal::ExceptionFlags;
using internal::RoundingControl;

// Implementing fenv.h functions when only x87 are available.

LIBC_INLINE static int clear_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  x87::clear_except(x86_excepts);
  return 0;
}

LIBC_INLINE static int test_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  uint16_t tested_excepts = x87::test_except(x86_excepts);
  return internal::get_macro_from_exception_status(tested_excepts);
}

LIBC_INLINE static int get_except() {
  uint16_t excepts = x87::get_except();
  return internal::get_macro_from_exception_status(excepts);
}

LIBC_INLINE static int set_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  x87::set_except(x86_excepts);
  return 0;
}

LIBC_INLINE static int raise_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  x87::raise_except(x86_excepts);
  return 0;
}

LIBC_INLINE static int enable_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  uint16_t old_excepts = x87::enable_except(x86_excepts);
  return internal::get_macro_from_exception_status(old_excepts);
}

LIBC_INLINE static int disable_except(int excepts) {
  uint16_t x86_excepts = internal::get_status_value_from_except(excepts);
  uint16_t old_excepts = x87::disable_except(x86_excepts);
  return internal::get_macro_from_exception_status(old_excepts);
}

LIBC_INLINE static int get_round() {
  uint16_t rounding_mode = x87::get_round();
  return internal::get_macro_from_rounding_control(rounding_mode);
}

LIBC_INLINE static int set_round(int rounding_mode) {
  uint16_t rounding = internal::get_rounding_control_from_macro(rounding_mode);
  if (LIBC_UNLIKELY(rounding == internal::RoundingControl::RC_ERROR))
    return -1;
  x87::set_round(rounding);
  return 0;
}

LIBC_INLINE static void get_env(fenv_t *env) {
  internal::X87StateDescriptor x87_state;
  x87::get_x87_state_descriptor(x87_state);
  if constexpr (sizeof(fenv_t) >= sizeof(internal::X87StateDescriptor)) {
    // When fenv_t is 28 bytes or more, we assume that the structure is simply
    // store the entire x87 fenv state descriptor (28 bytes) at the beginning of
    // the struct.
    cpp::bit_copy(x87_state, *env);
  } else {
    // When fenv_t is less than 28 bytes, we will assume that it is following
    // mxcsr structure, so we simply put x87 state descriptor to the first
    // 16-bit following mxcsr.
    uint16_t mxcsr = internal::x87_state_to_mxcsr(x87_state);
    const char *mxcsr_ptr = reinterpret_cast<const char *>(&mxcsr);
    char *env_ptr = reinterpret_cast<char *>(env);
    cpp::inline_copy<sizeof(uint16_t)>(mxcsr_ptr, env_ptr);
  }
}

LIBC_INLINE static int set_env(const fenv_t *env) {
  if (env == FE_DFL_ENV) {
    x87::initialize_x87_state();
    return 0;
  }

  internal::X87StateDescriptor x87_state;
  const char *fenv_ptr = reinterpret_cast<const char *>(env);
  if constexpr (sizeof(fenv_t) >= sizeof(internal::X87StateDescriptor)) {
    // When fenv_t is 28 bytes or more, we assume that the structure is simply
    // store the entire x87 fenv state descriptor (28 bytes) at the beginning of
    // the struct.
    char *x87_state_ptr = reinterpret_cast<char *>(&x87_state);
    cpp::inline_copy<sizeof(x87_state)>(fenv_ptr, x87_state_ptr);
  } else {
    // When fenv_t is less than 28 bytes, we will assume that it is following
    // mxcsr structure, so we simply put x87 state descriptor to the first
    // 16-bit following mxcsr.
    uint16_t mxcsr = 0;
    static_assert(sizeof(fenv_t) >= sizeof(mxcsr));
    cpp::inline_copy<sizeof(mxcsr)>(fenv_ptr, reinterpret_cast<char *>(&mxcsr));
    // We then load the current x87 state descriptor, then replace all
    // relevant bits with mxcsr data before writing them back.
    x87::get_x87_state_descriptor(x87_state);
    internal::mxcsr_to_x87_state(mxcsr, x87_state);
  }
  x87::write_x87_state_descriptor(x87_state);
  return 0;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X87_ONLY_H
