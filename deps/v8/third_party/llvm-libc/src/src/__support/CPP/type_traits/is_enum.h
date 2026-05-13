//===-- is_enum type_traits -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ENUM_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ENUM_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_enum
template <typename T> struct is_enum : bool_constant<__is_enum(T)> {};
template <typename T>
LIBC_INLINE_VAR constexpr bool is_enum_v = is_enum<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ENUM_H
