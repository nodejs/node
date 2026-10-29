//===-- is_function type_traits ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_FUNCTION_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_FUNCTION_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/is_const.h"
#include "src/__support/CPP/type_traits/is_reference.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_function
#if __has_builtin(__is_function)
template <typename T>
struct is_function : integral_constant<bool, __is_function(T)> {};
#else
template <typename T>
struct is_function
    : public bool_constant<!(is_reference_v<T> || is_const_v<const T>)> {};
#endif
template <class T>
LIBC_INLINE_VAR constexpr bool is_function_v = is_function<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_FUNCTION_H
