//===-- A self contained equivalent of <limits> ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_LIMITS_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_LIMITS_H

#include "hdr/limits_macros.h" // CHAR_BIT
#include "src/__support/CPP/type_traits/is_integral.h"
#include "src/__support/CPP/type_traits/is_signed.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

namespace internal {

template <typename T, bool is_integral> struct numeric_limits_impl {};

template <typename T> struct numeric_limits_impl<T, true> {
  LIBC_INLINE_VAR static constexpr bool is_signed = T(-1) < T(0);

  LIBC_INLINE_VAR static constexpr int digits =
      (CHAR_BIT * sizeof(T)) - is_signed;

  LIBC_INLINE static constexpr T min() {
    if constexpr (is_signed) {
      return T(T(1) << digits);
    } else {
      return 0;
    }
  }

  LIBC_INLINE static constexpr T max() {
    if constexpr (is_signed) {
      return T(T(~0) ^ min());
    } else {
      return T(~0);
    }
  }
};

} // namespace internal

template <typename T>
struct numeric_limits
    : public internal::numeric_limits_impl<T, is_integral_v<T>> {};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_LIMITS_H
