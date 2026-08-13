//===-- is_unsigned type_traits ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_UNSIGNED_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_UNSIGNED_H

#include "include/llvm-libc-macros/stdfix-macros.h"
#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/is_integral.h"
#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/type_traits/remove_cv.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

#ifndef LIBC_COMPILER_HAS_FIXED_POINT
template <typename T>
struct is_unsigned : bool_constant<(is_integral_v<T> && (T(-1) > T(0)))> {
  LIBC_INLINE constexpr operator bool() const { return is_unsigned::value; }
  LIBC_INLINE constexpr bool operator()() const { return is_unsigned::value; }
};
#else
template <typename T> struct is_unsigned {
private:
  template <typename Head, typename... Args>
  LIBC_INLINE static constexpr bool __is_unqualified_any_of() {
    return (... || is_same_v<remove_cv_t<Head>, Args>);
  }

public:
  LIBC_INLINE_VAR static constexpr bool value =
      (is_integral_v<T> && (T(-1) > T(0))) ||
      __is_unqualified_any_of<T, unsigned short fract, unsigned fract,
                              unsigned long fract, unsigned short accum,
                              unsigned accum, unsigned long accum,
                              unsigned short sat fract, unsigned sat fract,
                              unsigned long sat fract, unsigned short sat accum,
                              unsigned sat accum, unsigned long sat accum>();
  LIBC_INLINE constexpr operator bool() const { return is_unsigned::value; }
  LIBC_INLINE constexpr bool operator()() const { return is_unsigned::value; }
};
#endif // LIBC_COMPILER_HAS_FIXED_POINT
#if LIBC_HAS_VECTOR_TYPE
template <typename T, size_t N>
struct is_unsigned<T [[clang::ext_vector_type(N)]]> : bool_constant<false> {};
#endif

template <typename T>
LIBC_INLINE_VAR constexpr bool is_unsigned_v = is_unsigned<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_UNSIGNED_H
