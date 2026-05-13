//===-- nextafter implementation for x86 long double numbers ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEXTAFTERLONGDOUBLE_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEXTAFTERLONGDOUBLE_H

#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

#if !defined(LIBC_TARGET_ARCH_IS_X86)
#error "Invalid include"
#endif

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

LIBC_INLINE constexpr long double nextafter(long double from, long double to) {
  using FPBits = FPBits<long double>;
  FPBits from_bits(from);
  if (from_bits.is_nan())
    return from;

  FPBits to_bits(to);
  if (to_bits.is_nan())
    return to;

  if (from == to)
    return to;

  // Convert pseudo subnormal number to normal number.
  if (from_bits.get_implicit_bit() == 1 && from_bits.is_subnormal()) {
    from_bits.set_biased_exponent(1);
  }

  using StorageType = FPBits::StorageType;

  constexpr StorageType FRACTION_MASK = FPBits::FRACTION_MASK;
  // StorageType int_val = from_bits.uintval();
  if (from == 0.0l) { // +0.0 / -0.0
    from_bits = FPBits::min_subnormal(from > to ? Sign::NEG : Sign::POS);
  } else if (from < 0.0l) {
    if (to < from) { // toward -inf
      if (from_bits == FPBits::max_subnormal(Sign::NEG)) {
        // We deal with normal/subnormal boundary separately to avoid
        // dealing with the implicit bit.
        from_bits = FPBits::min_normal(Sign::NEG);
      } else if (from_bits.get_mantissa() == FRACTION_MASK) {
        from_bits.set_mantissa(0);
        // Incrementing exponent might overflow the value to infinity,
        // which is what is expected. Since NaNs are handling separately,
        // it will never overflow "beyond" infinity.
        from_bits.set_biased_exponent(from_bits.get_biased_exponent() + 1);
        if (from_bits.is_inf())
          raise_except_if_required(FE_OVERFLOW | FE_INEXACT);
        return from_bits.get_val();
      } else {
        from_bits = FPBits(StorageType(from_bits.uintval() + 1));
      }
    } else { // toward +inf
      if (from_bits == FPBits::min_normal(Sign::NEG)) {
        // We deal with normal/subnormal boundary separately to avoid
        // dealing with the implicit bit.
        from_bits = FPBits::max_subnormal(Sign::NEG);
      } else if (from_bits.get_mantissa() == 0) {
        from_bits.set_mantissa(FRACTION_MASK);
        // from == 0 is handled separately so decrementing the exponent will not
        // lead to underflow.
        from_bits.set_biased_exponent(from_bits.get_biased_exponent() - 1);
        return from_bits.get_val();
      } else {
        from_bits = FPBits(StorageType(from_bits.uintval() - 1));
      }
    }
  } else {
    if (to < from) { // toward -inf
      if (from_bits == FPBits::min_normal(Sign::POS)) {
        from_bits = FPBits::max_subnormal(Sign::POS);
      } else if (from_bits.get_mantissa() == 0) {
        from_bits.set_mantissa(FRACTION_MASK);
        // from == 0 is handled separately so decrementing the exponent will not
        // lead to underflow.
        from_bits.set_biased_exponent(from_bits.get_biased_exponent() - 1);
        return from_bits.get_val();
      } else {
        from_bits = FPBits(StorageType(from_bits.uintval() - 1));
      }
    } else { // toward +inf
      if (from_bits == FPBits::max_subnormal(Sign::POS)) {
        from_bits = FPBits::min_normal(Sign::POS);
      } else if (from_bits.get_mantissa() == FRACTION_MASK) {
        from_bits.set_mantissa(0);
        // Incrementing exponent might overflow the value to infinity,
        // which is what is expected. Since NaNs are handling separately,
        // it will never overflow "beyond" infinity.
        from_bits.set_biased_exponent(from_bits.get_biased_exponent() + 1);
        if (from_bits.is_inf())
          raise_except_if_required(FE_OVERFLOW | FE_INEXACT);
        return from_bits.get_val();
      } else {
        from_bits = FPBits(StorageType(from_bits.uintval() + 1));
      }
    }
  }

  if (!from_bits.get_implicit_bit())
    raise_except_if_required(FE_UNDERFLOW | FE_INEXACT);

  return from_bits.get_val();
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEXTAFTERLONGDOUBLE_H
