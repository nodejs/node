//===-- nextupdown implementation for x86 long double numbers ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEXTUPDOWNLONGDOUBLE_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEXTUPDOWNLONGDOUBLE_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

#if !defined(LIBC_TARGET_ARCH_IS_X86)
#error "Invalid include"
#endif

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

template <bool IsDown>
LIBC_INLINE constexpr long double nextupdown(long double x) {
  constexpr Sign sign = IsDown ? Sign::NEG : Sign::POS;

  using FPBits_t = FPBits<long double>;
  FPBits_t xbits(x);
  if (xbits.is_nan() || xbits == FPBits_t::max_normal(sign) ||
      xbits == FPBits_t::inf(sign))
    return x;

  if (x == 0.0l)
    return FPBits_t::min_subnormal(sign).get_val();

  using StorageType = typename FPBits_t::StorageType;

  if (xbits.sign() == sign) {
    if (xbits.get_mantissa() == FPBits_t::FRACTION_MASK) {
      xbits.set_mantissa(0);
      xbits.set_biased_exponent(xbits.get_biased_exponent() + 1);
    } else {
      xbits = FPBits_t(StorageType(xbits.uintval() + 1));
    }

    return xbits.get_val();
  }

  if (xbits.get_mantissa() == 0) {
    xbits.set_mantissa(FPBits_t::FRACTION_MASK);
    xbits.set_biased_exponent(xbits.get_biased_exponent() - 1);
  } else {
    xbits = FPBits_t(StorageType(xbits.uintval() - 1));
  }

  return xbits.get_val();
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_NEXTUPDOWNLONGDOUBLE_H
