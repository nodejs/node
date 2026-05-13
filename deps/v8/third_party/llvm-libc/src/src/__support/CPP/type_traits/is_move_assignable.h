//===-- is_move_assignable type_traits --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_MOVE_ASSIGNABLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_MOVE_ASSIGNABLE_H

#include "src/__support/CPP/type_traits/add_lvalue_reference.h"
#include "src/__support/CPP/type_traits/add_rvalue_reference.h"
#include "src/__support/CPP/type_traits/integral_constant.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is move assignable
template <class T>
struct is_move_assignable
    : public integral_constant<bool, __is_assignable(
                                         cpp::add_lvalue_reference_t<T>,
                                         cpp::add_rvalue_reference_t<T>)> {};

template <class T>
LIBC_INLINE_VAR constexpr bool is_move_assignable_v =
    is_move_assignable<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_MOVE_ASSIGNABLE_H
