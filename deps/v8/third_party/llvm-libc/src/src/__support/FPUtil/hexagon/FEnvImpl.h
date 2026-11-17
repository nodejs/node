//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the Hexagon implementation of floating-point
/// environment manipulation functions.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_HEXAGON_FENVIMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_HEXAGON_FENVIMPL_H

#include "hdr/fenv_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/macros/attributes.h" // For LIBC_INLINE_ASM
#include "src/__support/macros/config.h"     // For LIBC_INLINE

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// Hexagon keeps all floating-point state in the User Status Register (USR):
//
//   bits  5:1   sticky exception status flags
//                 bit 1  FPINVF  invalid
//                 bit 2  FPDBZF  divide-by-zero
//                 bit 3  FPOVFF  overflow
//                 bit 4  FPUNFF  underflow
//                 bit 5  FPINPF  inexact
//   bits 23:22   FPRND   rounding mode
//                 00 round to nearest (ties to even)
//                 01 toward zero
//                 10 downward (toward -inf)
//                 11 upward (toward +inf)
//   bits 29:25   exception trap-enable bits (status flag << 24)
//                 bit 25 FPINVE, bit 26 FPDBZE, bit 27 FPOVFE,
//                 bit 28 FPUNFE, bit 29 FPINEE
//
// Note the hardware status/enable bit positions do NOT match the FE_* macro
// values used by llvm-libc, so we translate between the two below.
struct FEnv {
  // Hardware status-flag bit positions within USR.
  static constexpr uint32_t INVALID = 0x02;   // bit 1
  static constexpr uint32_t DIVBYZERO = 0x04; // bit 2
  static constexpr uint32_t OVERFLOW = 0x08;  // bit 3
  static constexpr uint32_t UNDERFLOW = 0x10; // bit 4
  static constexpr uint32_t INEXACT = 0x20;   // bit 5
  static constexpr uint32_t STATUS_MASK =
      INVALID | DIVBYZERO | OVERFLOW | UNDERFLOW | INEXACT;

  // The trap-enable bits sit STATUS bits shifted up by this amount.
  static constexpr uint32_t ENABLE_SHIFT = 24;
  static constexpr uint32_t ENABLE_MASK = STATUS_MASK << ENABLE_SHIFT;

  // Rounding mode field.
  static constexpr uint32_t RND_SHIFT = 22;
  static constexpr uint32_t RND_MASK = 0x3;
  static constexpr uint32_t RND_TONEAREST = 0x0;
  static constexpr uint32_t RND_TOWARDZERO = 0x1;
  static constexpr uint32_t RND_DOWNWARD = 0x2;
  static constexpr uint32_t RND_UPWARD = 0x3;

  // Combined mask of all FP-relevant USR bits.  fegetenv/fesetenv only touch
  // these so that non-FP USR fields (loop counters, prefetch state, etc.) are
  // never disturbed.
  static constexpr uint32_t FP_MASK =
      STATUS_MASK | (RND_MASK << RND_SHIFT) | ENABLE_MASK;

  LIBC_INLINE static uint32_t get_usr() {
    uint32_t usr;
    LIBC_INLINE_ASM("%0 = usr\n\t" : "=r"(usr));
    return usr;
  }

  LIBC_INLINE static void set_usr(uint32_t usr) {
    LIBC_INLINE_ASM("usr = %0\n\t" : : "r"(usr));
  }

  // Translate a set of FE_* exception macros into hardware status-flag bits.
  LIBC_INLINE static uint32_t exception_macro_to_bits(int except) {
    return ((except & FE_INVALID) ? INVALID : 0) |
           ((except & FE_DIVBYZERO) ? DIVBYZERO : 0) |
           ((except & FE_OVERFLOW) ? OVERFLOW : 0) |
           ((except & FE_UNDERFLOW) ? UNDERFLOW : 0) |
           ((except & FE_INEXACT) ? INEXACT : 0);
  }

  // Translate hardware status-flag bits back into FE_* exception macros.
  LIBC_INLINE static int exception_bits_to_macro(uint32_t status) {
    return ((status & INVALID) ? FE_INVALID : 0) |
           ((status & DIVBYZERO) ? FE_DIVBYZERO : 0) |
           ((status & OVERFLOW) ? FE_OVERFLOW : 0) |
           ((status & UNDERFLOW) ? FE_UNDERFLOW : 0) |
           ((status & INEXACT) ? FE_INEXACT : 0);
  }
};

LIBC_INLINE int enable_except(int excepts) {
  uint32_t usr = FEnv::get_usr();
  uint32_t old_enabled = (usr >> FEnv::ENABLE_SHIFT) & FEnv::STATUS_MASK;
  uint32_t new_enabled = old_enabled | FEnv::exception_macro_to_bits(excepts);
  FEnv::set_usr((usr & ~FEnv::ENABLE_MASK) |
                (new_enabled << FEnv::ENABLE_SHIFT));
  return FEnv::exception_bits_to_macro(old_enabled);
}

LIBC_INLINE int disable_except(int excepts) {
  uint32_t usr = FEnv::get_usr();
  uint32_t old_enabled = (usr >> FEnv::ENABLE_SHIFT) & FEnv::STATUS_MASK;
  uint32_t new_enabled = old_enabled & ~FEnv::exception_macro_to_bits(excepts);
  FEnv::set_usr((usr & ~FEnv::ENABLE_MASK) |
                (new_enabled << FEnv::ENABLE_SHIFT));
  return FEnv::exception_bits_to_macro(old_enabled);
}

LIBC_INLINE int get_except() {
  uint32_t enabled =
      (FEnv::get_usr() >> FEnv::ENABLE_SHIFT) & FEnv::STATUS_MASK;
  return FEnv::exception_bits_to_macro(enabled);
}

LIBC_INLINE int clear_except(int excepts) {
  uint32_t usr = FEnv::get_usr();
  usr &= ~FEnv::exception_macro_to_bits(excepts);
  FEnv::set_usr(usr);
  return 0;
}

LIBC_INLINE int test_except(int excepts) {
  uint32_t to_test = FEnv::exception_macro_to_bits(excepts);
  uint32_t status = FEnv::get_usr() & FEnv::STATUS_MASK;
  return FEnv::exception_bits_to_macro(status & to_test);
}

LIBC_INLINE int set_except(int excepts) {
  uint32_t usr = FEnv::get_usr();
  FEnv::set_usr(usr | FEnv::exception_macro_to_bits(excepts));
  return 0;
}

LIBC_INLINE int raise_except(int excepts) {
  // Setting the sticky status flag triggers a trap if the corresponding
  // enable bit is set; otherwise it simply records the exception.
  uint32_t usr = FEnv::get_usr();
  FEnv::set_usr(usr | FEnv::exception_macro_to_bits(excepts));
  return 0;
}

LIBC_INLINE int get_round() {
  uint32_t rm = (FEnv::get_usr() >> FEnv::RND_SHIFT) & FEnv::RND_MASK;
  switch (rm) {
  case FEnv::RND_TONEAREST:
    return FE_TONEAREST;
  case FEnv::RND_TOWARDZERO:
    return FE_TOWARDZERO;
  case FEnv::RND_DOWNWARD:
    return FE_DOWNWARD;
  case FEnv::RND_UPWARD:
    return FE_UPWARD;
  default:
    return -1; // Error value.
  }
}

LIBC_INLINE int set_round(int mode) {
  uint32_t rm;
  switch (mode) {
  case FE_TONEAREST:
    rm = FEnv::RND_TONEAREST;
    break;
  case FE_TOWARDZERO:
    rm = FEnv::RND_TOWARDZERO;
    break;
  case FE_DOWNWARD:
    rm = FEnv::RND_DOWNWARD;
    break;
  case FE_UPWARD:
    rm = FEnv::RND_UPWARD;
    break;
  default:
    return -1; // To indicate failure.
  }
  uint32_t usr = FEnv::get_usr();
  FEnv::set_usr((usr & ~(FEnv::RND_MASK << FEnv::RND_SHIFT)) |
                (rm << FEnv::RND_SHIFT));
  return 0;
}

LIBC_INLINE int get_env(fenv_t *envp) {
  uint32_t *state = reinterpret_cast<uint32_t *>(envp);
  *state = FEnv::get_usr() & FEnv::FP_MASK;
  return 0;
}

LIBC_INLINE int set_env(const fenv_t *envp) {
  uint32_t usr = FEnv::get_usr();
  if (envp == FE_DFL_ENV) {
    // Default environment: round to nearest, no exceptions raised or enabled.
    FEnv::set_usr(usr & ~FEnv::FP_MASK);
    return 0;
  }
  uint32_t state = *reinterpret_cast<const uint32_t *>(envp);
  // Preserve non-FP USR bits; only load the FP-relevant fields.
  FEnv::set_usr((usr & ~FEnv::FP_MASK) | (state & FEnv::FP_MASK));
  return 0;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_HEXAGON_FENVIMPL_H
