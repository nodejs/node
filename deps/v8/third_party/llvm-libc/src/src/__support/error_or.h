//===-- A data structure for returning an error or a value ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_ERROR_OR_H
#define LLVM_LIBC_SRC___SUPPORT_ERROR_OR_H

#include "src/__support/CPP/expected.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

template <class T> using ErrorOr = cpp::expected<T, int>;

using Error = cpp::unexpected<int>;

// template <typename T> struct ErrorOr {
//   union {
//     T value;
//     int error;
//   };
//   bool is_error;

//   constexpr ErrorOr(T value) : value(value), is_error(false) {}
//   constexpr ErrorOr(int error, bool is_error)
//       : error(error), is_error(is_error) {}

//   constexpr bool has_error() { return is_error; }

//   constexpr operator bool() { return is_error; }
//   constexpr operator T() { return value; }
// };

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_ERROR_OR_H
