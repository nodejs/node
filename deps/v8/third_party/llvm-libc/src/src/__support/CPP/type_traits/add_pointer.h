//===-- add_pointer type_traits ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ADD_POINTER_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ADD_POINTER_H

#include "src/__support/CPP/type_traits/remove_reference.h"
#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// add_pointer
namespace detail {
template <class T>
auto try_add_pointer(int) -> cpp::type_identity<cpp::remove_reference_t<T> *>;
template <class T> auto try_add_pointer(...) -> cpp::type_identity<T>;
} // namespace detail
template <class T>
struct add_pointer : decltype(detail::try_add_pointer<T>(0)) {};
template <class T> using add_pointer_t = typename add_pointer<T>::type;
} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_ADD_POINTER_H
