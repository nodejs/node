//===-- x87 floating point env manipulation utilities -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X87_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X87_UTILS_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/fenv_t.h"
#include "src/__support/FPUtil/x86_64/fenv_x86_common.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/sanitizer.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace x87 {

using internal::ExceptionFlags;
using internal::RoundingControl;
using internal::X87StateDescriptor;

LIBC_INLINE static uint16_t get_x87_control_word() {
  uint16_t w;

#ifdef LIBC_COMPILER_IS_MSVC
  __asm fstcw w;
#else  // !LIBC_COMPILER_IS_MSVC
  asm volatile("fnstcw %0" : "=m"(w)::);
  MSAN_UNPOISON(&w, sizeof(w));
#endif // LIBC_COMPILER_IS_MSVC

  return w;
}

LIBC_INLINE static void write_x87_control_word(uint16_t w) {
#ifdef LIBC_COMPILER_IS_MSVC
  __asm fldcw w;
#else  // !LIBC_COMPILER_IS_MSVC
  asm volatile("fldcw %0" : : "m"(w) :);
#endif // LIBC_COMPILER_IS_MSVC
}

LIBC_INLINE static uint16_t get_x87_status_word() {
  uint16_t w;

#ifdef LIBC_COMPILER_IS_MSVC
  __asm fnstsw w;
#else  // !LIBC_COMPILER_IS_MSVC
  asm volatile("fnstsw %0" : "=m"(w)::);
  MSAN_UNPOISON(&w, sizeof(w));
#endif // LIBC_COMPILER_IS_MSVC

  return w;
}

LIBC_INLINE static void clear_x87_exceptions() {
#ifdef LIBC_COMPILER_IS_MSVC
  __asm fnclex;
#else  // !LIBC_COMPILER_IS_MSVC
  asm volatile("fnclex" : : :);
#endif // LIBC_COMPILER_IS_MSVC
}

LIBC_INLINE static void get_x87_state_descriptor(X87StateDescriptor &s) {
#ifdef LIBC_COMPILER_IS_MSVC
  __asm fnstenv s;
#else  // !LIBC_COMPILER_IS_MSVC
  asm volatile("fnstenv %0" : "=m"(s));
  MSAN_UNPOISON(&s, sizeof(s));
#endif // LIBC_COMPILER_IS_MSVC
}

LIBC_INLINE static void
write_x87_state_descriptor(const X87StateDescriptor &s) {
#ifdef LIBC_COMPILER_IS_MSVC
  __asm fldenv s;
#else  // !LIBC_COMPILER_IS_MSVC
  asm volatile("fldenv %0" : : "m"(s) :);
#endif // LIBC_COMPILER_IS_MSVC
}

LIBC_INLINE static void initialize_x87_state() {
#ifdef LIBC_COMPILER_IS_MSVC
  __asm fninit;
#else  // !LIBC_COMPILER_IS_MSVC
  asm volatile("fninit" : : :);
#endif // LIBC_COMPILER_IS_MSVC
}

LIBC_INLINE static void clear_except(uint16_t excepts) {
  if (excepts == ExceptionFlags::ALL_F) {
    clear_x87_exceptions();
    return;
  }

  X87StateDescriptor x87_descriptor;
  get_x87_state_descriptor(x87_descriptor);
  x87_descriptor.status_word &= static_cast<uint16_t>(~excepts);
  write_x87_state_descriptor(x87_descriptor);
}

LIBC_INLINE static uint16_t test_except(uint16_t excepts) {
  uint16_t x87_status = get_x87_status_word();
  return static_cast<uint16_t>(x87_status & excepts);
}

LIBC_INLINE static uint16_t get_except() {
  uint16_t x87_control = get_x87_control_word();
  return static_cast<uint16_t>((~x87_control) & ExceptionFlags::ALL_F);
}

LIBC_INLINE static void set_except(uint16_t excepts) {
  X87StateDescriptor x87_descriptor;
  get_x87_state_descriptor(x87_descriptor);
  uint16_t current_excepts =
      static_cast<uint16_t>(x87_descriptor.status_word & ExceptionFlags::ALL_F);
  // Do nothing if excepts are unchanged.
  if (current_excepts == excepts)
    return;
  // Clear excepts.
  x87_descriptor.status_word &= static_cast<uint16_t>(~ExceptionFlags::ALL_F);
  // Set excepts.
  x87_descriptor.status_word |= excepts;
  write_x87_state_descriptor(x87_descriptor);
}

LIBC_INLINE static void raise_except(uint16_t excepts) {
  X87StateDescriptor x87_descriptor;
  get_x87_state_descriptor(x87_descriptor);
  uint16_t current_excepts =
      static_cast<uint16_t>(x87_descriptor.status_word & ExceptionFlags::ALL_F);
  // Do nothing if excepts are unchanged.
  if ((current_excepts | excepts) == current_excepts)
    return;
  // Update excepts.
  x87_descriptor.status_word |= excepts;
  write_x87_state_descriptor(x87_descriptor);
}

LIBC_INLINE static uint16_t enable_except(uint16_t excepts) {
  uint16_t x87_control = get_x87_control_word();
  uint16_t old_excepts =
      static_cast<uint16_t>(~x87_control & ExceptionFlags::ALL_F);
  // Only update if excepts are not enabled.
  if ((excepts | old_excepts) != old_excepts) {
    x87_control &= static_cast<uint16_t>(~excepts);
    write_x87_control_word(x87_control);
  }
  return old_excepts;
}

LIBC_INLINE static uint16_t disable_except(uint16_t excepts) {
  uint16_t x87_control = get_x87_control_word();
  uint16_t old_excepts =
      static_cast<uint16_t>(~x87_control & ExceptionFlags::ALL_F);
  // Only update excepts if some of the excepts are enabled.
  if ((x87_control | excepts) != x87_control) {
    x87_control |= excepts;
    write_x87_control_word(x87_control);
  }
  return old_excepts;
}

LIBC_INLINE static uint16_t get_round() {
  uint16_t x87_control = get_x87_control_word();
  return static_cast<uint16_t>(
      (x87_control >> RoundingControl::X87_BIT_POSITION) &
      RoundingControl::ROUNDING_MASK);
}

LIBC_INLINE static void set_round(uint16_t rounding_mode) {
  uint16_t x87_control = get_x87_control_word();
  rounding_mode <<= RoundingControl::X87_BIT_POSITION;
  uint16_t x87_control_new =
      (x87_control & (~RoundingControl::X87_ROUNDING_MASK)) | rounding_mode;
  // Only update if rounding mode changes.
  if (x87_control_new != x87_control)
    write_x87_control_word(x87_control_new);
}

} // namespace x87

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_FENV_X87_UTILS_H
