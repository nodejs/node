//===-- forward utility -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_FORWARD_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_FORWARD_H

#include "src/__support/CPP/type_traits/is_lvalue_reference.h"
#include "src/__support/CPP/type_traits/remove_reference.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// forward
template <typename T>
LIBC_INLINE constexpr T &&forward(remove_reference_t<T> &value) {
  return static_cast<T &&>(value);
}

template <typename T>
LIBC_INLINE constexpr T &&forward(remove_reference_t<T> &&value) {
  static_assert(!is_lvalue_reference_v<T>,
                "cannot forward an rvalue as an lvalue");
  return static_cast<T &&>(value);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_UTILITY_FORWARD_H
