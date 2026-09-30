//===-- is_signed type_traits -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_SIGNED_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_SIGNED_H

#include "include/llvm-libc-macros/stdfix-macros.h"
#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/is_arithmetic.h"
#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/type_traits/remove_cv.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

#ifndef LIBC_COMPILER_HAS_FIXED_POINT
template <typename T>
struct is_signed : bool_constant<(is_arithmetic_v<T> && (T(-1) < T(0)))> {
  LIBC_INLINE constexpr operator bool() const { return is_signed::value; }
  LIBC_INLINE constexpr bool operator()() const { return is_signed::value; }
};
#else
template <typename T> struct is_signed {
private:
  template <typename Head, typename... Args>
  LIBC_INLINE static constexpr bool __is_unqualified_any_of() {
    return (... || is_same_v<remove_cv_t<Head>, Args>);
  }

public:
  LIBC_INLINE_VAR static constexpr bool value =
      (is_arithmetic_v<T> && (T(-1) < T(0))) ||
      __is_unqualified_any_of<T, short fract, fract, long fract, short accum,
                              accum, long accum, short sat fract, sat fract,
                              long sat fract, short sat accum, sat accum,
                              long sat accum>();
  LIBC_INLINE constexpr operator bool() const { return is_signed::value; }
  LIBC_INLINE constexpr bool operator()() const { return is_signed::value; }
};
#endif // LIBC_COMPILER_HAS_FIXED_POINT

template <typename T>
LIBC_INLINE_VAR constexpr bool is_signed_v = is_signed<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_SIGNED_H
