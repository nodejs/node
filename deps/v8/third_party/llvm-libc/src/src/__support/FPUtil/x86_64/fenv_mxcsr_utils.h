//===-- sse2 floating point env manipulation utilities ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_MXCSR_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_MXCSR_UTILS_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/x86_64/fenv_x86_common.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/sanitizer.h"

#include <immintrin.h>

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace sse {

using internal::ExceptionFlags;
using internal::RoundingControl;

// SSE FPU environment from Intel 64 and IA-32 Architectures Software Developer
// Manuals - Chapter 10
// https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
//
// The SSE floating point environment will be save/load with LDMXCSR/STMXCSR
// instructions, which will return the following 4-byte structure in 32-bit
// mode (see section 10.2.3, figure 10-3 in the manual linked above).

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

LIBC_INLINE static uint32_t get_mxcsr() { return _mm_getcsr(); }

LIBC_INLINE static void write_mxcsr(uint32_t w) { _mm_setcsr(w); }

LIBC_INLINE static void clear_except(uint16_t excepts) {
  uint32_t mxcsr = get_mxcsr();
  mxcsr &= ~static_cast<uint32_t>(excepts);
  write_mxcsr(mxcsr);
}

LIBC_INLINE static uint16_t test_except(uint16_t excepts) {
  uint32_t mxcsr = get_mxcsr();
  return static_cast<uint16_t>(excepts & ExceptionFlags::ALL_F & mxcsr);
}

LIBC_INLINE static uint16_t get_except() {
  uint32_t mxcsr = ~get_mxcsr();
  return static_cast<uint16_t>(
      (mxcsr >> ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION) &
      ExceptionFlags::ALL_F);
}

LIBC_INLINE static void set_except(uint16_t excepts) {
  _MM_SET_EXCEPTION_STATE(excepts);
}

LIBC_INLINE static void raise_except(uint16_t excepts) {
  uint32_t mxcsr = get_mxcsr();
  mxcsr |= excepts & ExceptionFlags::ALL_F;
  write_mxcsr(mxcsr);
#ifdef LIBC_TRAP_ON_RAISE_FP_EXCEPT
  // We will try to trigger the SIGFPE if floating point exceptions are not
  // masked.  Since we already set all the floating point exception flags, we
  // only need to trigger the trap on one of them.
  static constexpr float EXCEPTION_INPUTS[6][2] = {
      // FE_INVALID: 0.0 * inf
      {0.0f, cpp::bit_cast<float>(0x7f80'0000U)},
      // FE_DENORM: 1.0 * 0x1.0p-128
      {1.0f, 0x1.0p-128f},
      // FE_DIVBYZERO: 1.0 / 0.0
      {1.0f, 0.0f},
      // FE_OVERFLOW: 0x1.0p127 * 0x1.0p127
      {0x1.0p127f, 0x1.0p127f},
      // FE_UNDERFLOW: 0x1.0p-126 * 0x1.0p-126
      {0x1.0p-126f, 0x1.0p-126f},
      // FE_INEXACT: (1 + 2^-12) * (1 + 2^-12)
      {0x1.001p0f, 0x1.001p0f}};

  uint32_t except_masks =
      (~(get_mxcsr() >> ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION)) &
      excepts;
  if (except_masks) {
    int idx = cpp::countr_zero(except_masks);
    if (idx == 2) {
      // FE_DIVBYZERO, we need floating point division operations.
      [[maybe_unused]] volatile float z = EXCEPTION_INPUTS[idx][0];
      z /= EXCEPTION_INPUTS[idx][1];
    } else {
      // For the remaining exceptions, we use floating point multiplications.
      [[maybe_unused]] volatile float z = EXCEPTION_INPUTS[idx][0];
      z *= EXCEPTION_INPUTS[idx][1];
    }
  }
#endif // LIBC_TRAP_ON_RAISE_FP_EXCEPT
}

LIBC_INLINE static uint16_t enable_except(uint16_t excepts) {
  uint32_t mxcsr = get_mxcsr();
  uint16_t old_excepts =
      (mxcsr >> ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION) &
      ExceptionFlags::ALL_F;
  mxcsr &= ~(static_cast<uint32_t>(excepts)
             << ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION);
  write_mxcsr(mxcsr);
  return old_excepts;
}

LIBC_INLINE static uint16_t disable_except(uint16_t excepts) {
  uint32_t mxcsr = get_mxcsr();
  uint16_t old_excepts =
      (mxcsr >> ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION) &
      ExceptionFlags::ALL_F;
  mxcsr |= (static_cast<uint32_t>(excepts)
            << ExceptionFlags::MXCSR_EXCEPTION_MASK_BIT_POSITION);
  write_mxcsr(mxcsr);
  return old_excepts;
}

LIBC_INLINE static uint16_t get_round() {
  uint32_t mxcsr = get_mxcsr();
  return static_cast<uint16_t>(mxcsr >> RoundingControl::MXCSR_BIT_POSITION) &
         RoundingControl::ROUNDING_MASK;
}

LIBC_INLINE static void set_round(uint16_t rounding_mode) {
  uint32_t mxcsr = get_mxcsr();
  rounding_mode <<= RoundingControl::MXCSR_BIT_POSITION;
  // Clear rounding bits.
  mxcsr &= (~RoundingControl::MXCSR_ROUNDING_MASK);
  write_mxcsr(mxcsr | rounding_mode);
}

} // namespace sse

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_MXCSR_UTILS_H
