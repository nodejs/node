//===-- aarch64 floating point env manipulation functions -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_FENVIMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_FENVIMPL_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

#if !defined(LIBC_TARGET_ARCH_IS_AARCH64) || defined(__APPLE__)
#error "Invalid include"
#endif

#include <arm_acle.h>

#include "hdr/fenv_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/FPUtil/FPBits.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {
struct FEnv {
  struct FPState {
    uint32_t ControlWord;
    uint32_t StatusWord;
  };

  static_assert(
      sizeof(fenv_t) == sizeof(FPState),
      "Internal floating point state does not match the public fenv_t type.");

  static constexpr uint32_t TONEAREST = 0x0;
  static constexpr uint32_t UPWARD = 0x1;
  static constexpr uint32_t DOWNWARD = 0x2;
  static constexpr uint32_t TOWARDZERO = 0x3;

  static constexpr uint32_t INVALID_F = 0x1;
  static constexpr uint32_t DIVBYZERO_F = 0x2;
  static constexpr uint32_t OVERFLOW_F = 0x4;
  static constexpr uint32_t UNDERFLOW_F = 0x8;
  static constexpr uint32_t INEXACT_F = 0x10;

  // Zero-th bit is the first bit.
  static constexpr uint32_t RoundingControlBitPosition = 22;
  static constexpr uint32_t ExceptionStatusFlagsBitPosition = 0;
  static constexpr uint32_t ExceptionControlFlagsBitPosition = 8;

  LIBC_INLINE static uint32_t getStatusValueForExcept(int excepts) {
    return ((excepts & FE_INVALID) ? INVALID_F : 0) |
           ((excepts & FE_DIVBYZERO) ? DIVBYZERO_F : 0) |
           ((excepts & FE_OVERFLOW) ? OVERFLOW_F : 0) |
           ((excepts & FE_UNDERFLOW) ? UNDERFLOW_F : 0) |
           ((excepts & FE_INEXACT) ? INEXACT_F : 0);
  }

  LIBC_INLINE static int exceptionStatusToMacro(uint32_t status) {
    return ((status & INVALID_F) ? FE_INVALID : 0) |
           ((status & DIVBYZERO_F) ? FE_DIVBYZERO : 0) |
           ((status & OVERFLOW_F) ? FE_OVERFLOW : 0) |
           ((status & UNDERFLOW_F) ? FE_UNDERFLOW : 0) |
           ((status & INEXACT_F) ? FE_INEXACT : 0);
  }

  static uint32_t getControlWord() {
#ifdef __clang__
    // GCC does not currently support __arm_rsr.
    return __arm_rsr("fpcr");
#else
    return __builtin_aarch64_get_fpcr();
#endif
  }

  static void writeControlWord(uint32_t fpcr) {
#ifdef __clang__
    // GCC does not currently support __arm_wsr.
    __arm_wsr("fpcr", fpcr);
#else
    __builtin_aarch64_set_fpcr(fpcr);
#endif
  }

  static uint32_t getStatusWord() {
#ifdef __clang__
    return __arm_rsr("fpsr");
#else
    return __builtin_aarch64_get_fpsr();
#endif
  }

  static void writeStatusWord(uint32_t fpsr) {
#ifdef __clang__
    __arm_wsr("fpsr", fpsr);
#else
    __builtin_aarch64_set_fpsr(fpsr);
#endif
  }
};

LIBC_INLINE int enable_except(int excepts) {
  uint32_t newExcepts = FEnv::getStatusValueForExcept(excepts);
  uint32_t controlWord = FEnv::getControlWord();
  int oldExcepts =
      (controlWord >> FEnv::ExceptionControlFlagsBitPosition) & 0x1F;
  controlWord |= (newExcepts << FEnv::ExceptionControlFlagsBitPosition);
  FEnv::writeControlWord(controlWord);
  return FEnv::exceptionStatusToMacro(static_cast<uint32_t>(oldExcepts));
}

LIBC_INLINE int disable_except(int excepts) {
  uint32_t disabledExcepts = FEnv::getStatusValueForExcept(excepts);
  uint32_t controlWord = FEnv::getControlWord();
  int oldExcepts =
      (controlWord >> FEnv::ExceptionControlFlagsBitPosition) & 0x1F;
  controlWord &= ~(disabledExcepts << FEnv::ExceptionControlFlagsBitPosition);
  FEnv::writeControlWord(controlWord);
  return FEnv::exceptionStatusToMacro(static_cast<uint32_t>(oldExcepts));
}

LIBC_INLINE int get_except() {
  uint32_t controlWord = FEnv::getControlWord();
  uint32_t enabledExcepts =
      (controlWord >> FEnv::ExceptionControlFlagsBitPosition) & 0x1F;
  return FEnv::exceptionStatusToMacro(enabledExcepts);
}

LIBC_INLINE int clear_except(int excepts) {
  uint32_t statusWord = FEnv::getStatusWord();
  uint32_t toClear = FEnv::getStatusValueForExcept(excepts);
  statusWord &= ~(toClear << FEnv::ExceptionStatusFlagsBitPosition);
  FEnv::writeStatusWord(statusWord);
  return 0;
}

LIBC_INLINE int test_except(int excepts) {
  uint32_t toTest = FEnv::getStatusValueForExcept(excepts);
  uint32_t statusWord = FEnv::getStatusWord();
  return FEnv::exceptionStatusToMacro(
      (statusWord >> FEnv::ExceptionStatusFlagsBitPosition) & toTest);
}

LIBC_INLINE int set_except(int excepts) {
  uint32_t statusWord = FEnv::getStatusWord();
  uint32_t statusValue = FEnv::getStatusValueForExcept(excepts);
  statusWord |= (statusValue << FEnv::ExceptionStatusFlagsBitPosition);
  FEnv::writeStatusWord(statusWord);
  return 0;
}

LIBC_INLINE int raise_except(int excepts) {
  float zero = 0.0f;
  float one = 1.0f;
  float largeValue = FPBits<float>::max_normal().get_val();
  float smallValue = FPBits<float>::min_normal().get_val();
  auto divfunc = [](float a, float b) {
    __asm__ __volatile__("ldr  s0, %0\n\t"
                         "ldr  s1, %1\n\t"
                         "fdiv s0, s0, s1\n\t"
                         : // No outputs
                         : "m"(a), "m"(b)
                         : "s0", "s1" /* s0 and s1 are clobbered */);
  };

  uint32_t toRaise = FEnv::getStatusValueForExcept(excepts);
  int result = 0;

  if (toRaise & FEnv::INVALID_F) {
    divfunc(zero, zero);
    uint32_t statusWord = FEnv::getStatusWord();
    if (!((statusWord >> FEnv::ExceptionStatusFlagsBitPosition) &
          FEnv::INVALID_F))
      result = -1;
  }

  if (toRaise & FEnv::DIVBYZERO_F) {
    divfunc(one, zero);
    uint32_t statusWord = FEnv::getStatusWord();
    if (!((statusWord >> FEnv::ExceptionStatusFlagsBitPosition) &
          FEnv::DIVBYZERO_F))
      result = -1;
  }
  if (toRaise & FEnv::OVERFLOW_F) {
    divfunc(largeValue, smallValue);
    uint32_t statusWord = FEnv::getStatusWord();
    if (!((statusWord >> FEnv::ExceptionStatusFlagsBitPosition) &
          FEnv::OVERFLOW_F))
      result = -1;
  }
  if (toRaise & FEnv::UNDERFLOW_F) {
    divfunc(smallValue, largeValue);
    uint32_t statusWord = FEnv::getStatusWord();
    if (!((statusWord >> FEnv::ExceptionStatusFlagsBitPosition) &
          FEnv::UNDERFLOW_F))
      result = -1;
  }
  if (toRaise & FEnv::INEXACT_F) {
    float two = 2.0f;
    float three = 3.0f;
    // 2.0 / 3.0 cannot be represented exactly in any radix 2 floating point
    // format.
    divfunc(two, three);
    uint32_t statusWord = FEnv::getStatusWord();
    if (!((statusWord >> FEnv::ExceptionStatusFlagsBitPosition) &
          FEnv::INEXACT_F))
      result = -1;
  }
  return result;
}

LIBC_INLINE int get_round() {
  uint32_t roundingMode =
      (FEnv::getControlWord() >> FEnv::RoundingControlBitPosition) & 0x3;
  switch (roundingMode) {
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
}

LIBC_INLINE int set_round(int mode) {
  uint16_t bitValue;
  switch (mode) {
  case FE_TONEAREST:
    bitValue = FEnv::TONEAREST;
    break;
  case FE_DOWNWARD:
    bitValue = FEnv::DOWNWARD;
    break;
  case FE_UPWARD:
    bitValue = FEnv::UPWARD;
    break;
  case FE_TOWARDZERO:
    bitValue = FEnv::TOWARDZERO;
    break;
  default:
    return 1; // To indicate failure
  }

  uint32_t controlWord = FEnv::getControlWord();
  controlWord &=
      static_cast<uint32_t>(~(0x3 << FEnv::RoundingControlBitPosition));
  controlWord |=
      static_cast<uint32_t>(bitValue << FEnv::RoundingControlBitPosition);
  FEnv::writeControlWord(controlWord);

  return 0;
}

LIBC_INLINE int get_env(fenv_t *envp) {
  FEnv::FPState *state = reinterpret_cast<FEnv::FPState *>(envp);
  state->ControlWord = FEnv::getControlWord();
  state->StatusWord = FEnv::getStatusWord();
  return 0;
}

LIBC_INLINE int set_env(const fenv_t *envp) {
  if (envp == FE_DFL_ENV) {
    // Default status and control words bits are all zeros so we just
    // write zeros.
    FEnv::writeStatusWord(0);
    FEnv::writeControlWord(0);
    return 0;
  }
  const FEnv::FPState *state = reinterpret_cast<const FEnv::FPState *>(envp);
  FEnv::writeControlWord(state->ControlWord);
  FEnv::writeStatusWord(state->StatusWord);
  return 0;
}
} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_AARCH64_FENVIMPL_H
