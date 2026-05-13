//===-- is_complex type_traits ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_COMPLEX_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_COMPLEX_H

#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/CPP/type_traits/remove_cv.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
// LIBC_TYPES_HAS_CFLOAT16 && LIBC_TYPES_HAS_CFLOAT128
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/complex_types.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_complex
#ifdef LIBC_COMPILER_IS_MSVC
// TODO: Add support for complex types with MSVC.
template <typename T> struct is_complex : false_type {};
#else
template <typename T> struct is_complex {
private:
  template <typename Head, typename... Args>
  LIBC_INLINE_VAR static constexpr bool __is_unqualified_any_of() {
    return (... || is_same_v<remove_cv_t<Head>, Args>);
  }

public:
  LIBC_INLINE_VAR static constexpr bool value =
      __is_unqualified_any_of<T, _Complex float, _Complex double,
                              _Complex long double
#ifdef LIBC_TYPES_HAS_CFLOAT16
                              ,
                              cfloat16
#endif
#ifdef LIBC_TYPES_HAS_CFLOAT128
                              ,
                              cfloat128
#endif
                              >();
};
#endif // LIBC_COMPILER_IS_MSVC

template <typename T>
LIBC_INLINE_VAR constexpr bool is_complex_v = is_complex<T>::value;
template <typename T1, typename T2>
LIBC_INLINE_VAR constexpr bool is_complex_type_same() {
  return is_same_v<remove_cv_t<T1>, T2>;
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_COMPLEX_H
