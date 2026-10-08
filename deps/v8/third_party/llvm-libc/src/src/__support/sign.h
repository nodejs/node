//===-- A simple sign type --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_SIGN_H
#define LLVM_LIBC_SRC___SUPPORT_SIGN_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE, LIBC_INLINE_VAR

namespace LIBC_NAMESPACE_DECL {

// A type to interact with signed arithmetic types.
struct Sign {
  LIBC_INLINE constexpr bool is_pos() const { return !is_negative; }
  LIBC_INLINE constexpr bool is_neg() const { return is_negative; }

  LIBC_INLINE friend constexpr bool operator==(Sign a, Sign b) {
    return a.is_negative == b.is_negative;
  }

  LIBC_INLINE friend constexpr bool operator!=(Sign a, Sign b) {
    return !(a == b);
  }

  static const Sign POS;
  static const Sign NEG;

  LIBC_INLINE constexpr Sign negate() const { return Sign(!is_negative); }

private:
  LIBC_INLINE constexpr explicit Sign(bool is_negative)
      : is_negative(is_negative) {}

  bool is_negative;
};

LIBC_INLINE_VAR constexpr Sign Sign::NEG = Sign(true);
LIBC_INLINE_VAR constexpr Sign Sign::POS = Sign(false);

} // namespace LIBC_NAMESPACE_DECL
#endif // LLVM_LIBC_SRC___SUPPORT_SIGN_H
