//===-- is_scalar type_traits -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_SCALAR_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_SCALAR_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/is_arithmetic.h"
#include "src/__support/CPP/type_traits/is_enum.h"
#include "src/__support/CPP/type_traits/is_member_pointer.h"
#include "src/__support/CPP/type_traits/is_null_pointer.h"
#include "src/__support/CPP/type_traits/is_pointer.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_scalar
template <class T>
struct is_scalar
    : cpp::bool_constant<cpp::is_arithmetic_v<T> || cpp::is_enum_v<T> ||
                         cpp::is_pointer_v<T> || cpp::is_member_pointer_v<T> ||
                         cpp::is_null_pointer_v<T>> {};
template <class T>
LIBC_INLINE_VAR constexpr bool is_scalar_v = is_scalar<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_SCALAR_H
