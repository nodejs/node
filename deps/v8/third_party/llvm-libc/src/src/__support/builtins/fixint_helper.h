//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Shared truncating float-to-integer conversions, saturating on overflow.
/// These mirror compiler-rt's __fix<f><i> / __fixuns<f><i> builtins via an
/// FPBits unpack + shift, so they can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXINT_HELPER_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXINT_HELPER_H

#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// TODO: use fputil::round_to_signed_integer after adding Float128/64/32/16
// classes currently, we use these helpers to avoid an infinite loop that
// happens because fputil::round_to_signed_integer calls the builtins if
// the target doesn't have FPU.

// Truncating conversion of F to the signed integer I (round toward zero).
// Out-of-range magnitudes saturate to I's min/max; mirrors compiler-rt __fix*.
template <typename I, typename F> LIBC_INLINE constexpr I fixint(F a) {
  using FPBits = fputil::FPBits<F>;
  using UI = cpp::make_unsigned_t<I>;
  constexpr I FIXINT_MAX = cpp::numeric_limits<I>::max();
  constexpr I FIXINT_MIN = cpp::numeric_limits<I>::min();

  const FPBits bits(a);
  const I sign = bits.is_neg() ? -1 : 1;
  const int exponent = bits.get_exponent();
  const typename FPBits::StorageType significand = bits.get_explicit_mantissa();

  if (exponent < 0)
    return 0;

  if (static_cast<unsigned>(exponent) >= sizeof(I) * 8)
    return sign == 1 ? FIXINT_MAX : FIXINT_MIN;

  if (exponent < FPBits::FRACTION_LEN)
    return sign *
           static_cast<I>(significand >> (FPBits::FRACTION_LEN - exponent));
  return sign * static_cast<I>(static_cast<UI>(significand)
                               << (exponent - FPBits::FRACTION_LEN));
}

// Truncating conversion of F to the unsigned integer U (round toward zero).
// Negative or out-of-range values yield 0 / U's max; mirrors compiler-rt
// __fixuns*.
template <typename U, typename F> LIBC_INLINE constexpr U fixuint(F a) {
  using FPBits = fputil::FPBits<F>;

  const FPBits bits(a);
  const int exponent = bits.get_exponent();
  const typename FPBits::StorageType significand = bits.get_explicit_mantissa();

  if (bits.is_neg() || exponent < 0)
    return 0;

  if (static_cast<unsigned>(exponent) >= sizeof(U) * 8)
    return ~U(0);

  if (exponent < FPBits::FRACTION_LEN)
    return static_cast<U>(significand >> (FPBits::FRACTION_LEN - exponent));
  return static_cast<U>(static_cast<U>(significand)
                        << (exponent - FPBits::FRACTION_LEN));
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FIXINT_HELPER_H
