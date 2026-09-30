//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Shared integer-to-floating-point conversions, rounded to nearest with ties
/// to even.  These mirror compiler-rt's __float<i><f> / __floatun<i><f>
/// builtins via an LLVM-libc DyadicFloat, so they can be reused by
/// compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FLOATINT_HELPER_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FLOATINT_HELPER_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/sign.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// TODO: use constructors after adding Float128/64/32/16 classes.

// Convert the integer I to the floating-point type F, rounding to nearest
// (ties to even) per the current rounding mode.  Handles signed and unsigned
// I alike; mirrors compiler-rt's __float<i><f> / __floatun<i><f>.
template <typename F, typename I> LIBC_INLINE constexpr F floatint(I x) {
  using UI = cpp::make_unsigned_t<I>;

  Sign sign = Sign::POS;
  UI mag = static_cast<UI>(x);
  if constexpr (cpp::is_signed_v<I>) {
    if (x < 0) {
      sign = Sign::NEG;
      mag = static_cast<UI>(-mag); // modular negation; correct for I's min too
    }
  }

  // A mantissa wide enough for I and for F's fraction, rounded up to a whole
  // number of 64-bit words (UInt's width requirement).  With exponent 0 the
  // dyadic value is exactly `mag`, which as<F>() then rounds to F.
  constexpr size_t NEED = (sizeof(I) > sizeof(F) ? sizeof(I) : sizeof(F)) * 8;
  constexpr size_t BITS = ((NEED + 63) / 64) * 64;
  return static_cast<F>(fputil::DyadicFloat<BITS>(sign, 0, mag));
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FLOATINT_HELPER_H
