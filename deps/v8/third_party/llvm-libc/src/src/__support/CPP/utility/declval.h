//===-- declval utility -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_DECLVAL_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_DECLVAL_H

#include "src/__support/CPP/type_traits/add_rvalue_reference.h"
#include "src/__support/CPP/type_traits/always_false.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// declval
template <typename T> cpp::add_rvalue_reference_t<T> declval() {
  static_assert(cpp::always_false<T>,
                "declval not allowed in an evaluated context");
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_DECLVAL_H
