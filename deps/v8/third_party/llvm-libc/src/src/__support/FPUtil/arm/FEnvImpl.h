//===-- arm floating point env manipulation functions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_ARM_FENVIMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_ARM_FENVIMPL_H

#include "hdr/fenv_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/attributes.h" // For LIBC_INLINE
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

struct FEnv {
  // Arm floating point state is all stored in a single 32-bit register named
  // fpscr.
  uint32_t fpscr;
  static constexpr uint32_t RoundingControlBitPosition = 22;
  static constexpr uint32_t ExceptionControlBitPosition = 8;

  static constexpr uint32_t TONEAREST = 0x0;
  static constexpr uint32_t UPWARD = 0x1;
  static constexpr uint32_t DOWNWARD = 0x2;
  static constexpr uint32_t TOWARDZERO = 0x3;

  static constexpr uint32_t INVALID_ENABLE = 0x1;
  static constexpr uint32_t DIVBYZERO_ENABLE = 0x2;
  static constexpr uint32_t OVERFLOW_ENABLE = 0x4;
  static constexpr uint32_t UNDERFLOW_ENABLE = 0x8;
  static constexpr uint32_t INEXACT_ENABLE = 0x10;
  static constexpr uint32_t DENORMAL_ENABLE = 0x20;

  static constexpr uint32_t INVALID_STATUS = 0x1;
  static constexpr uint32_t DIVBYZERO_STATUS = 0x2;
  static constexpr uint32_t OVERFLOW_STATUS = 0x4;
  static constexpr uint32_t UNDERFLOW_STATUS = 0x8;
  static constexpr uint32_t INEXACT_STATUS = 0x10;
  static constexpr uint32_t DENORMAL_STATUS = 0x80;

  LIBC_INLINE static uint32_t get_fpscr() { return __builtin_arm_get_fpscr(); }
  LIBC_INLINE static void set_fpscr(uint32_t val) {
    __builtin_arm_set_fpscr(val);
  }

  LIBC_INLINE static int exception_enable_bits_to_macro(uint32_t status) {
    return ((status & INVALID_ENABLE) ? FE_INVALID : 0) |
           ((status & DIVBYZERO_ENABLE) ? FE_DIVBYZERO : 0) |
           ((status & OVERFLOW_ENABLE) ? FE_OVERFLOW : 0) |
           ((status & UNDERFLOW_ENABLE) ? FE_UNDERFLOW : 0) |
           ((status & INEXACT_ENABLE) ? FE_INEXACT : 0);
  }

  LIBC_INLINE static uint32_t exception_macro_to_enable_bits(int except) {
    return ((except & FE_INVALID) ? INVALID_ENABLE : 0) |
           ((except & FE_DIVBYZERO) ? DIVBYZERO_ENABLE : 0) |
           ((except & FE_OVERFLOW) ? OVERFLOW_ENABLE : 0) |
           ((except & FE_UNDERFLOW) ? UNDERFLOW_ENABLE : 0) |
           ((except & FE_INEXACT) ? INEXACT_ENABLE : 0);
  }

  LIBC_INLINE static uint32_t exception_macro_to_status_bits(int except) {
    return ((except & FE_INVALID) ? INVALID_STATUS : 0) |
           ((except & FE_DIVBYZERO) ? DIVBYZERO_STATUS : 0) |
           ((except & FE_OVERFLOW) ? OVERFLOW_STATUS : 0) |
           ((except & FE_UNDERFLOW) ? UNDERFLOW_STATUS : 0) |
           ((except & FE_INEXACT) ? INEXACT_STATUS : 0);
  }

  LIBC_INLINE static uint32_t exception_status_bits_to_macro(int status) {
    return ((status & INVALID_STATUS) ? FE_INVALID : 0) |
           ((status & DIVBYZERO_STATUS) ? FE_DIVBYZERO : 0) |
           ((status & OVERFLOW_STATUS) ? FE_OVERFLOW : 0) |
           ((status & UNDERFLOW_STATUS) ? FE_UNDERFLOW : 0) |
           ((status & INEXACT_STATUS) ? FE_INEXACT : 0);
  }
};

// Enables exceptions in |excepts| and returns the previously set exceptions.
LIBC_INLINE int enable_except(int excepts) {
  uint32_t new_excepts = FEnv::exception_macro_to_enable_bits(excepts);
  uint32_t fpscr = FEnv::get_fpscr();
  int old = (fpscr >> FEnv::ExceptionControlBitPosition) & 0x3F;
  fpscr |= (new_excepts << FEnv::ExceptionControlBitPosition);
  FEnv::set_fpscr(fpscr);
  return FEnv::exception_enable_bits_to_macro(old);
}

// Disables exceptions in |excepts| and returns the previously set exceptions.
LIBC_INLINE int disable_except(int excepts) {
  uint32_t disable_bits = FEnv::exception_macro_to_enable_bits(excepts);
  uint32_t fpscr = FEnv::get_fpscr();
  int old = (fpscr >> FEnv::ExceptionControlBitPosition) & 0x3F;
  fpscr &= ~(disable_bits << FEnv::ExceptionControlBitPosition);
  FEnv::set_fpscr(fpscr);
  return FEnv::exception_enable_bits_to_macro(old);
}

// Returns the currently enabled exceptions.
LIBC_INLINE int get_except() {
  uint32_t fpscr = FEnv::get_fpscr();
  int enabled_excepts = (fpscr >> FEnv::ExceptionControlBitPosition) & 0x3F;
  return FEnv::exception_enable_bits_to_macro(enabled_excepts);
}

// Clears the exceptions in |excepts|.
LIBC_INLINE int clear_except(int excepts) {
  uint32_t fpscr = FEnv::get_fpscr();
  uint32_t to_clear = FEnv::exception_macro_to_status_bits(excepts);
  fpscr &= ~to_clear;
  FEnv::set_fpscr(fpscr);
  return 0;
}

// Returns the set of exceptions which are from the input set |excepts|.
LIBC_INLINE int test_except(int excepts) {
  uint32_t to_test = FEnv::exception_macro_to_status_bits(excepts);
  uint32_t fpscr = FEnv::get_fpscr();
  return FEnv::exception_status_bits_to_macro(fpscr & 0x9F & to_test);
}

// Set the exceptions in |excepts|.
LIBC_INLINE int set_except(int excepts) {
  uint32_t fpscr = FEnv::get_fpscr();
  FEnv::set_fpscr(fpscr | FEnv::exception_macro_to_status_bits(excepts));
  return 0;
}

LIBC_INLINE int raise_except(int excepts) {
  float zero = 0.0f;
  float one = 1.0f;
  float large_value = FPBits<float>::max_normal().get_val();
  float small_value = FPBits<float>::min_normal().get_val();
  auto divfunc = [](float a, float b) {
    __asm__ __volatile__("flds  s0, %0\n\t"
                         "flds  s1, %1\n\t"
                         "fdivs s0, s0, s1\n\t"
                         : // No outputs
                         : "m"(a), "m"(b)
                         : "s0", "s1" /* s0 and s1 are clobbered */);
  };

  uint32_t to_raise = FEnv::exception_macro_to_status_bits(excepts);
  int result = 0;

  if (to_raise & FEnv::INVALID_STATUS) {
    divfunc(zero, zero);
    uint32_t fpscr = FEnv::get_fpscr();
    if (!(fpscr & FEnv::INVALID_STATUS))
      result = -1;
  }
  if (to_raise & FEnv::DIVBYZERO_STATUS) {
    divfunc(one, zero);
    uint32_t fpscr = FEnv::get_fpscr();
    if (!(fpscr & FEnv::DIVBYZERO_STATUS))
      result = -1;
  }
  if (to_raise & FEnv::OVERFLOW_STATUS) {
    divfunc(large_value, small_value);
    uint32_t fpscr = FEnv::get_fpscr();
    if (!(fpscr & FEnv::OVERFLOW_STATUS))
      result = -1;
  }
  if (to_raise & FEnv::UNDERFLOW_STATUS) {
    divfunc(small_value, large_value);
    uint32_t fpscr = FEnv::get_fpscr();
    if (!(fpscr & FEnv::UNDERFLOW_STATUS))
      result = -1;
  }
  if (to_raise & FEnv::INEXACT_STATUS) {
    float two = 2.0f;
    float three = 3.0f;
    // 2.0 / 3.0 cannot be represented exactly in any radix 2 floating point
    // format.
    divfunc(two, three);
    uint32_t fpscr = FEnv::get_fpscr();
    if (!(fpscr & FEnv::INEXACT_STATUS))
      result = -1;
  }
  return result;
}

LIBC_INLINE int get_round() {
  uint32_t mode = (FEnv::get_fpscr() >> FEnv::RoundingControlBitPosition) & 0x3;
  switch (mode) {
  case FEnv::TONEAREST:
    return FE_TONEAREST;
  case FEnv::DOWNWARD:
    return FE_DOWNWARD;
  case FEnv::UPWARD:
    return FE_UPWARD;
  case FEnv::TOWARDZERO:
    return FE_TOWARDZERO;
  default:
    return -1; // Error value.
  }
  return 0;
}

LIBC_INLINE int set_round(int mode) {
  uint16_t bits;
  switch (mode) {
  case FE_TONEAREST:
    bits = FEnv::TONEAREST;
    break;
  case FE_DOWNWARD:
    bits = FEnv::DOWNWARD;
    break;
  case FE_UPWARD:
    bits = FEnv::UPWARD;
    break;
  case FE_TOWARDZERO:
    bits = FEnv::TOWARDZERO;
    break;
  default:
    return 1; // To indicate failure
  }

  uint32_t fpscr = FEnv::get_fpscr();
  fpscr &= ~(0x3 << FEnv::RoundingControlBitPosition);
  fpscr |= (bits << FEnv::RoundingControlBitPosition);
  FEnv::set_fpscr(fpscr);

  return 0;
}

LIBC_INLINE int get_env(fenv_t *envp) {
  FEnv *state = reinterpret_cast<FEnv *>(envp);
  state->fpscr = FEnv::get_fpscr();
  return 0;
}

LIBC_INLINE int set_env(const fenv_t *envp) {
  if (envp == FE_DFL_ENV) {
    uint32_t fpscr = FEnv::get_fpscr();
    // Default status implies:
    // 1. Round to nearest rounding mode.
    fpscr &= ~(0x3 << FEnv::RoundingControlBitPosition);
    fpscr |= (FEnv::TONEAREST << FEnv::RoundingControlBitPosition);
    // 2. All exceptions are disabled.
    fpscr &= ~(0x3F << FEnv::ExceptionControlBitPosition);
    // 3. All exceptions are cleared. There are two reserved bits
    // at bit 5 and 6 so we just write one full byte (6 bits for
    // the exceptions, and 2 reserved bits.)
    fpscr &= ~(static_cast<uint32_t>(0xFF));

    FEnv::set_fpscr(fpscr);
    return 0;
  }

  const FEnv *state = reinterpret_cast<const FEnv *>(envp);
  FEnv::set_fpscr(state->fpscr);
  return 0;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_ARM_FENVIMPL_H
