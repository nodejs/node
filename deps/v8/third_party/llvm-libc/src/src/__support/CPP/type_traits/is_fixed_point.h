//===-- is_fixed_point type_traits ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_FIXED_POINT_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_FIXED_POINT_H

#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/type_traits/remove_cv.h"
#include "src/__support/macros/attributes.h"

#include "include/llvm-libc-macros/stdfix-macros.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_fixed_point
#ifdef LIBC_COMPILER_HAS_FIXED_POINT
template <typename T> struct is_fixed_point {
private:
  template <typename Head, typename... Args>
  LIBC_INLINE static constexpr bool __is_unqualified_any_of() {
    return (... || is_same_v<remove_cv_t<Head>, Args>);
  }

public:
  LIBC_INLINE_VAR static constexpr bool value = __is_unqualified_any_of<
      T, short fract, fract, long fract, unsigned short fract, unsigned fract,
      unsigned long fract, short accum, accum, long accum, unsigned short accum,
      unsigned accum, unsigned long accum, short sat fract, sat fract,
      long sat fract, unsigned short sat fract, unsigned sat fract,
      unsigned long sat fract, short sat accum, sat accum, long sat accum,
      unsigned short sat accum, unsigned sat accum, unsigned long sat accum>();
};
#else
template <typename T> struct is_fixed_point : false_type {};
#endif // LIBC_COMPILER_HAS_FIXED_POINT

template <typename T>
LIBC_INLINE_VAR constexpr bool is_fixed_point_v = is_fixed_point<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_FIXED_POINT_H
