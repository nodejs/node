//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Floating-point integer check functions.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_IS_INTEGER_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_IS_INTEGER_H

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// These helpers require finite inputs.

// Check if x is an odd integer: its lowest set significand bit must be at the
// unit position, so x_e + lsb == UNIT_EXPONENT.
template <typename T> LIBC_INLINE constexpr bool is_odd_integer(T x) {
  using FPBits = FPBits<T>;
  using StorageType = typename FPBits::StorageType;
  FPBits xbits(x);
  StorageType x_u = xbits.uintval();
  unsigned x_e = static_cast<unsigned>(xbits.get_biased_exponent());
  unsigned lsb = static_cast<unsigned>(
      cpp::countr_zero(static_cast<StorageType>(x_u | FPBits::EXP_MASK)));
  constexpr unsigned UNIT_EXPONENT =
      static_cast<unsigned>(FPBits::EXP_BIAS + FPBits::FRACTION_LEN);
  return x_e + lsb == UNIT_EXPONENT;
}

// Check if x is an integer: its lowest set significand bit must be at or above
// the unit position, so x_e + lsb >= UNIT_EXPONENT.
template <typename T> LIBC_INLINE constexpr bool is_integer(T x) {
  using FPBits = FPBits<T>;
  using StorageType = typename FPBits::StorageType;
  FPBits xbits(x);
  if (xbits.is_zero())
    return true;
  StorageType x_u = xbits.uintval();
  unsigned x_e = static_cast<unsigned>(xbits.get_biased_exponent());
  unsigned lsb = static_cast<unsigned>(
      cpp::countr_zero(static_cast<StorageType>(x_u | FPBits::EXP_MASK)));
  constexpr unsigned UNIT_EXPONENT =
      static_cast<unsigned>(FPBits::EXP_BIAS + FPBits::FRACTION_LEN);
  return x_e + lsb >= UNIT_EXPONENT;
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_IS_INTEGER_H
