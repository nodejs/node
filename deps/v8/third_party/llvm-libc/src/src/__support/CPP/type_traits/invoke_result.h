//===-- invoke_result type_traits -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INVOKE_RESULT_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INVOKE_RESULT_H

#include "src/__support/CPP/type_traits/invoke.h"
#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/CPP/utility/declval.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <class F, class... Args>
struct invoke_result : cpp::type_identity<decltype(cpp::invoke(
                           cpp::declval<F>(), cpp::declval<Args>()...))> {};

template <class F, class... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INVOKE_RESULT_H
