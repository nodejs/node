//===-- riscv floating point env manipulation functions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_RISCV_FENVIMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_RISCV_FENVIMPL_H

#include "hdr/fenv_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/attributes.h" // For LIBC_INLINE_ASM
#include "src/__support/macros/config.h"     // For LIBC_INLINE

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

struct FEnv {
  // We will ignore RMM and DYN rounding modes.
  static constexpr uint32_t TONEAREST = 0x0;
  static constexpr uint32_t TOWARDZERO = 0x1;
  static constexpr uint32_t DOWNWARD = 0x2;
  static constexpr uint32_t UPWARD = 0x3;

  // These are the bit locations of the corresponding exceptions in fcsr.
  static constexpr uint32_t INEXACT = 0x1;
  static constexpr uint32_t UNDERFLOW = 0x2;
  static constexpr uint32_t OVERFLOW = 0x4;
  static constexpr uint32_t DIVBYZERO = 0x8;
  static constexpr uint32_t INVALID = 0x10;

  LIBC_INLINE static uint32_t get_frm() {
    unsigned int rm;
    LIBC_INLINE_ASM("frrm %0\n\t" : "=r"(rm));
    return rm;
  }

  LIBC_INLINE static void set_frm(uint32_t rm) {
    LIBC_INLINE_ASM("fsrm %0, %0\n\t" : "+r"(rm));
  }

  LIBC_INLINE static uint32_t get_fflags() {
    unsigned int flags;
    LIBC_INLINE_ASM("frflags %0\n\t" : "=r"(flags));
    return flags;
  }

  LIBC_INLINE static void set_fflags(uint32_t flags) {
    LIBC_INLINE_ASM("fsflags %0, %0\n\t" : "+r"(flags));
  }

  LIBC_INLINE static uint32_t get_fcsr() {
    unsigned int fcsr;
    LIBC_INLINE_ASM("frcsr %0\n\t" : "=r"(fcsr));
    return fcsr;
  }

  LIBC_INLINE static void set_fcsr(uint32_t fcsr) {
    LIBC_INLINE_ASM("fscsr %0, %0\n\t" : "+r"(fcsr));
  }

  LIBC_INLINE static int exception_bits_to_macro(uint32_t status) {
    return ((status & INVALID) ? FE_INVALID : 0) |
           ((status & DIVBYZERO) ? FE_DIVBYZERO : 0) |
           ((status & OVERFLOW) ? FE_OVERFLOW : 0) |
           ((status & UNDERFLOW) ? FE_UNDERFLOW : 0) |
           ((status & INEXACT) ? FE_INEXACT : 0);
  }

  LIBC_INLINE static uint32_t exception_macro_to_bits(int except) {
    return ((except & FE_INVALID) ? INVALID : 0) |
           ((except & FE_DIVBYZERO) ? DIVBYZERO : 0) |
           ((except & FE_OVERFLOW) ? OVERFLOW : 0) |
           ((except & FE_UNDERFLOW) ? UNDERFLOW : 0) |
           ((except & FE_INEXACT) ? INEXACT : 0);
  }
};

// Since RISCV does not have exception enable bits, we will just return
// the failure indicator.
LIBC_INLINE int enable_except(int) { return -1; }

// Always succeed.
LIBC_INLINE int disable_except(int) { return 0; }

// Always return "no exceptions enabled".
LIBC_INLINE int get_except() { return 0; }

LIBC_INLINE int clear_except(int excepts) {
  uint32_t flags = FEnv::get_fflags();
  uint32_t to_clear = FEnv::exception_macro_to_bits(excepts);
  flags &= ~to_clear;
  FEnv::set_fflags(flags);
  return 0;
}

LIBC_INLINE int test_except(int excepts) {
  uint32_t to_test = FEnv::exception_macro_to_bits(excepts);
  uint32_t flags = FEnv::get_fflags();
  return FEnv::exception_bits_to_macro(flags & to_test);
}

LIBC_INLINE int set_except(int excepts) {
  uint32_t flags = FEnv::get_fflags();
  FEnv::set_fflags(flags | FEnv::exception_macro_to_bits(excepts));
  return 0;
}

LIBC_INLINE int raise_except(int excepts) {
  // Since there are no traps, we just set the exception flags.
  uint32_t flags = FEnv::get_fflags();
  FEnv::set_fflags(flags | FEnv::exception_macro_to_bits(excepts));
  return 0;
}

LIBC_INLINE int get_round() {
  uint32_t rm = FEnv::get_frm();
  switch (rm) {
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
  uint32_t rm;
  switch (mode) {
  case FE_TONEAREST:
    rm = FEnv::TONEAREST;
    break;
  case FE_DOWNWARD:
    rm = FEnv::DOWNWARD;
    break;
  case FE_UPWARD:
    rm = FEnv::UPWARD;
    break;
  case FE_TOWARDZERO:
    rm = FEnv::TOWARDZERO;
    break;
  default:
    return -1; // To indicate failure
  }
  FEnv::set_frm(rm);
  return 0;
}

LIBC_INLINE int get_env(fenv_t *envp) {
  uint32_t *state = reinterpret_cast<uint32_t *>(envp);
  *state = FEnv::get_fcsr();
  return 0;
}

LIBC_INLINE int set_env(const fenv_t *envp) {
  if (envp == FE_DFL_ENV) {
    FEnv::set_frm(FEnv::TONEAREST);
    FEnv::set_fflags(0);
    return 0;
  }
  uint32_t status = *reinterpret_cast<const uint32_t *>(envp);
  // We have to do the masking to preserve the reserved bits.
  FEnv::set_fcsr((status & 0xFF) | (FEnv::get_fcsr() & 0xFFFFFF00));
  return 0;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_RISCV_FENVIMPL_H
