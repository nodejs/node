//===-- void_t type_traits --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_VOID_T_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_VOID_T_H

#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// void_t

namespace detail {
template <typename... Ts> struct make_void : cpp::type_identity<void> {};
} // namespace detail

template <typename... Ts>
using void_t = typename detail::make_void<Ts...>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_VOID_T_H
