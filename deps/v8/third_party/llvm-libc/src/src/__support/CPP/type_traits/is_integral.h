//===-- is_integral type_traits ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_INTEGRAL_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_INTEGRAL_H

#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/type_traits/remove_cv.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/types.h" // LIBC_TYPES_HAS_INT128

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_integral
template <typename T> struct is_integral {
private:
  template <typename Head, typename... Args>
  LIBC_INLINE_VAR static constexpr bool __is_unqualified_any_of() {
    return (... || is_same_v<remove_cv_t<Head>, Args>);
  }

public:
  LIBC_INLINE_VAR static constexpr bool value =
      __is_unqualified_any_of<T,
#ifdef LIBC_TYPES_HAS_INT128
                              __int128_t, __uint128_t,
#endif
#ifdef __cpp_char8_t
                              char8_t,
#endif
                              char16_t, char32_t, char, signed char,
                              unsigned char, short, unsigned short, int,
                              unsigned int, long, unsigned long, long long,
                              unsigned long long, bool>();
};
template <typename T>
LIBC_INLINE_VAR constexpr bool is_integral_v = is_integral<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_INTEGRAL_H
