//===-- x87 floating point env manipulation functions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X86_COMMON_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X86_COMMON_H

#include <stdbool.h>

#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace internal {

// Default order of floating point exception flags in x87 and mxcsr registers:
// - Bit 0: Invalid Operations
// - Bit 1: Denormal
// - Bit 2: Divide-by-zero
// - Bit 3: Overflow
// - Bit 4: Underflow
// - Bit 5: Inexact
struct ExceptionFlags {
  static constexpr uint16_t INVALID_F = 0x1;
  // Some libcs define __FE_DENORM corresponding to the denormal input
  // exception and include it in FE_ALL_EXCEPTS. We define and use it to
  // support compiling against headers provided by such libcs.
  static constexpr uint16_t DENORMAL_F = 0x2;
  static constexpr uint16_t DIV_BY_ZERO_F = 0x4;
  static constexpr uint16_t OVERFLOW_F = 0x8;
  static constexpr uint16_t UNDERFLOW_F = 0x10;
  static constexpr uint16_t INEXACT_F = 0x20;
  static constexpr uint16_t ALL_F =
      static_cast<uint16_t>(INVALID_F | DENORMAL_F | DIV_BY_ZERO_F |
                            OVERFLOW_F | UNDERFLOW_F | INEXACT_F);
  static constexpr unsigned MXCSR_EXCEPTION_MASK_BIT_POSITION = 7;
};

LIBC_INLINE static constexpr bool fenv_exceptions_match_x86() {
  return (FE_INVALID == ExceptionFlags::INVALID_F) &&
#ifdef __FE_DENORM
         (__FE_DENORM == ExceptionFlags::DENORMAL_F) &&
#elif defined(FE_DENORM)
         (FE_DENORM == ExceptionFlags::DENORMAL_F) &&
#endif // __FE_DENORM
         (FE_DIVBYZERO == ExceptionFlags::DIV_BY_ZERO_F) &&
         (FE_OVERFLOW == ExceptionFlags::OVERFLOW_F) &&
         (FE_UNDERFLOW == ExceptionFlags::UNDERFLOW_F) &&
         (FE_INEXACT == ExceptionFlags::INEXACT_F);
}

// The rounding control values in the x87 control register and the MXCSR
// register have the same 2-bit enoding but have different bit positions.
// See below for the bit positions.
struct RoundingControl {
  static constexpr uint16_t TO_NEAREST = 0x0;
  static constexpr uint16_t DOWNWARD = 0x1;
  static constexpr uint16_t UPWARD = 0x2;
  static constexpr uint16_t TOWARD_ZERO = 0x3;
  static constexpr uint16_t ROUNDING_MASK = 0x3;
  static constexpr unsigned X87_BIT_POSITION = 10;
  static constexpr unsigned MXCSR_BIT_POSITION = 13;
  static constexpr uint16_t X87_ROUNDING_MASK = ROUNDING_MASK
                                                << X87_BIT_POSITION;
  static constexpr uint16_t MXCSR_ROUNDING_MASK = ROUNDING_MASK
                                                  << MXCSR_BIT_POSITION;
  static constexpr uint16_t RC_ERROR = 0xFFFF;
};

// Exception flags are individual bits in the corresponding registers.
// So, we just OR the bit values to get the full set of exceptions.
LIBC_INLINE static uint16_t get_status_value_from_except(int excepts) {
  if constexpr (fenv_exceptions_match_x86()) {
    return static_cast<uint16_t>(excepts & ExceptionFlags::ALL_F);
  } else {
    // We will make use of the fact that exception control bits are single
    // bit flags in the control registers.
    return ((excepts & FE_INVALID) ? ExceptionFlags::INVALID_F : 0) |
#ifdef __FE_DENORM
           ((excepts & __FE_DENORM) ? ExceptionFlags::DENORMAL_F : 0) |
#elif defined(FE_DENORM)
           ((excepts & FE_DENORM) ? ExceptionFlags::DENORMAL_F : 0) |
#endif // __FE_DENORM
           ((excepts & FE_DIVBYZERO) ? ExceptionFlags::DIV_BY_ZERO_F : 0) |
           ((excepts & FE_OVERFLOW) ? ExceptionFlags::OVERFLOW_F : 0) |
           ((excepts & FE_UNDERFLOW) ? ExceptionFlags::UNDERFLOW_F : 0) |
           ((excepts & FE_INEXACT) ? ExceptionFlags::INEXACT_F : 0);
  }
}

LIBC_INLINE static int get_macro_from_exception_status(uint16_t status) {
  if constexpr (fenv_exceptions_match_x86()) {
    return status & ExceptionFlags::ALL_F;
  } else {
    return ((status & ExceptionFlags::INVALID_F) ? FE_INVALID : 0) |
#ifdef __FE_DENORM
           ((status & ExceptionFlags::DENORMAL_F) ? __FE_DENORM : 0) |
#elif defined(FE_DENORM)
           ((status & ExceptionFlags::DENORMAL_F) ? FE_DENORM : 0) |
#endif // __FE_DENORM
           ((status & ExceptionFlags::DIV_BY_ZERO_F) ? FE_DIVBYZERO : 0) |
           ((status & ExceptionFlags::OVERFLOW_F) ? FE_OVERFLOW : 0) |
           ((status & ExceptionFlags::UNDERFLOW_F) ? FE_UNDERFLOW : 0) |
           ((status & ExceptionFlags::INEXACT_F) ? FE_INEXACT : 0);
  }
}

LIBC_INLINE static uint16_t get_rounding_control_from_macro(int rounding) {
  switch (rounding) {
  case FE_TONEAREST:
    return RoundingControl::TO_NEAREST;
  case FE_DOWNWARD:
    return RoundingControl::DOWNWARD;
  case FE_UPWARD:
    return RoundingControl::UPWARD;
  case FE_TOWARDZERO:
    return RoundingControl::TOWARD_ZERO;
  default:
    return RoundingControl::RC_ERROR;
  }
}

LIBC_INLINE static int get_macro_from_rounding_control(uint16_t rounding) {
  switch (rounding) {
  case RoundingControl::TO_NEAREST:
    return FE_TONEAREST;
  case RoundingControl::DOWNWARD:
    return FE_DOWNWARD;
  case RoundingControl::UPWARD:
    return FE_UPWARD;
  case RoundingControl::TOWARD_ZERO:
    return FE_TOWARDZERO;
  default:
    return -1;
  }
}

// x87 FPU environment from Intel 64 and IA-32 Architectures Software Developer
// Manuals - Chapter 8
// https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
//
// The x87 floating point environment will be save/load with FNSTENV/FLDENV
// instructions, which will return the following 28-byte structure in 32-bit
// mode (see section 8.1.10, figures 8-9 and 8-10 in the manual linked above),
// in which we only use the control and status words.

// x87 control word (16-bit) structure: (section 8.1.5 in the manual)
// - Bit 0: Invalid Exception Mask
// - Bit 1: Denormal Exception Mask
// - Bit 2: Division-by-zero Exception Mask
// - Bit 3: Overflow Exception Mask
// - Bit 4: Underflow Exception Mask
// - Bit 5: Inexact Exception Mask
// - Bit 6-7: Reserved
// - Bit 8-9: Precision Control
//            00 - Single Precision
//            01 - Reserved
//            10 - Double Precision
//            11 - Double Extended Precision (default)
// - Bit 10-11: Rounding Control
//            00 - Round to nearest, tie to even
//            01 - Round down (toward -inf)
//            10 - Round up (toward +inf)
//            11 - Round toward zero (truncate)
// - Bit 13-15: Reserved

// x87 status word (16-bit) structure: (section 8.1.3 in the manual)
// - Bit 0: Invalid Exception
// - Bit 1: Denormal Exception
// - Bit 2: Division-by-zero Exception
// - Bit 3: Overflow Exception
// - Bit 4: Underflow Exception
// - Bit 5: Inexact Exception
// - Bit 6: Stack Fault
// - Bit 7 Exception Summary Status
// - Bit 8-10: Condition Code
// - Bit 11-13: Top-of-stack Pointer
// - Bit 14: Condition Code
// - Bit 15: FPU Busy Flag
struct X87StateDescriptor {
  uint16_t control_word;
  uint16_t unused1;
  uint16_t status_word;
  uint16_t unused2;
  uint32_t _[5];
};

// Putting x87 state descriptor to mxcsr.
// SSE MXCSR register (32-bit) structure: (section 10.2.3 in the manual)
// - Bit 0: Invalid Exception
// - Bit 1: Denormal Exception
// - Bit 2: Division-by-zero Exception
// - Bit 3: Overflow Exception
// - Bit 4: Underflow Exception
// - Bit 5: Inexact Exception
// - Bit 6: Denormal Are Zeros (DAZ)
// - Bit 7: Invalid Exception Mask
// - Bit 8: Denormal Exception Mask
// - Bit 9: Division-by-zero Exception Mask
// - Bit 10: Overflow Exception Mask
// - Bit 11: Underflow Exception Mask
// - Bit 12: Inexact Exception Mask
// - Bit 13-14: Rounding Control
// - Bit 15: Flush Denormal To Zero (FTZ)
// - Bit 16-31: Reserved, will raise general-protection exception if set to
//              non-zero.
// For all of the following exception functions, we assume the excepts are
// normalized according to x86 and mxcsr exceptions defined in
// fenv_x86_common.h: ExceptionFlags.
LIBC_INLINE static uint16_t x87_state_to_mxcsr(const X87StateDescriptor &s) {
  uint16_t mxcsr = 0;
  // Copy 6 exception flags from status word.
  mxcsr = s.status_word & ExceptionFlags::ALL_F;
  // Copy 6 exception masks from control word.
  mxcsr |= (s.control_word & ExceptionFlags::ALL_F)
           << ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION;
  // Copy 2-bit rounding control.
  mxcsr |= (s.control_word & RoundingControl::X87_ROUNDING_MASK)
           << (RoundingControl::MXCSR_BIT_POSITION -
               RoundingControl::X87_BIT_POSITION);
  return mxcsr;
}

LIBC_INLINE static void mxcsr_to_x87_state(uint16_t mxcsr,
                                           X87StateDescriptor &s) {
  // Clear exception mask and rounding control.
  s.control_word &=
      ~(ExceptionFlags::ALL_F | RoundingControl::X87_ROUNDING_MASK);
  // Copy 6 exception masks.
  s.control_word |=
      (mxcsr >> ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION) &
      ExceptionFlags::ALL_F;
  // Copy rounding control.
  s.control_word |=
      (mxcsr & RoundingControl::MXCSR_ROUNDING_MASK) >>
      (RoundingControl::MXCSR_BIT_POSITION - RoundingControl::X87_BIT_POSITION);
  // Clear exception flags
  s.status_word &= ~ExceptionFlags::ALL_F;
  // Copy 6 exception status flags.
  s.status_word |= mxcsr & ExceptionFlags::ALL_F;
}

} // namespace internal

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X86_COMMON_H
