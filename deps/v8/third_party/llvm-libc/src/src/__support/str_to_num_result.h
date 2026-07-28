//===-- A data structure for str_to_number to return ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// -----------------------------------------------------------------------------
//                               **** WARNING ****
// This file is shared with libc++. You should also be careful when adding
// dependencies to this file, since it needs to build for all libc++ targets.
// -----------------------------------------------------------------------------

#ifndef LLVM_LIBC_SRC___SUPPORT_STR_TO_NUM_RESULT_H
#define LLVM_LIBC_SRC___SUPPORT_STR_TO_NUM_RESULT_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

// -----------------------------------------------------------------------------
//                               **** WARNING ****
// This interface is shared with libc++, if you change this interface you need
// to update it in both libc and libc++.
// -----------------------------------------------------------------------------
template <typename T> struct StrToNumResult {
  T value;
  int error;
  ptrdiff_t parsed_len;

  LIBC_INLINE constexpr StrToNumResult(T value)
      : value(value), error(0), parsed_len(0) {}
  LIBC_INLINE constexpr StrToNumResult(T value, ptrdiff_t parsed_len)
      : value(value), error(0), parsed_len(parsed_len) {}
  LIBC_INLINE constexpr StrToNumResult(T value, ptrdiff_t parsed_len, int error)
      : value(value), error(error), parsed_len(parsed_len) {}

  LIBC_INLINE constexpr bool has_error() { return error != 0; }

  LIBC_INLINE constexpr operator T() { return value; }
};
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_STR_TO_NUM_RESULT_H
