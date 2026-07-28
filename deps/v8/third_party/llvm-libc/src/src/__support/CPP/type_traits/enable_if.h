//===-- enable_if type_traits -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ENABLE_IF_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ENABLE_IF_H

#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// enable_if
template <bool B, typename T = void> struct enable_if;
template <typename T> struct enable_if<true, T> : type_identity<T> {};
template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ENABLE_IF_H
