//===--- Time units conversion ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TIME_UNITS_H
#define LLVM_LIBC_SRC___SUPPORT_TIME_UNITS_H

#include "hdr/types/time_t.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace time_units {
LIBC_INLINE constexpr time_t operator""_s_ns(unsigned long long s) {
  return static_cast<time_t>(s * 1'000'000'000);
}
LIBC_INLINE constexpr time_t operator""_s_us(unsigned long long s) {
  return static_cast<time_t>(s * 1'000'000);
}
LIBC_INLINE constexpr time_t operator""_s_ms(unsigned long long s) {
  return static_cast<time_t>(s * 1'000);
}
LIBC_INLINE constexpr time_t operator""_ms_ns(unsigned long long ms) {
  return static_cast<time_t>(ms * 1'000'000);
}
LIBC_INLINE constexpr time_t operator""_ms_us(unsigned long long ms) {
  return static_cast<time_t>(ms * 1'000);
}
LIBC_INLINE constexpr time_t operator""_us_ns(unsigned long long us) {
  return static_cast<time_t>(us * 1'000);
}
} // namespace time_units
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TIME_UNITS_H
