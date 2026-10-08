//===-- is_const type_traits ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONST_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONST_H

#include "src/__support/CPP/type_traits/false_type.h"
#include "src/__support/CPP/type_traits/true_type.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_const
template <class T> struct is_const : cpp::false_type {};
template <class T> struct is_const<const T> : cpp::true_type {};
template <class T>
LIBC_INLINE_VAR constexpr bool is_const_v = is_const<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONST_H
