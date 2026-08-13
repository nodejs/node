//===-- Implementation header for nanl --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_NANL_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_NANL_H

#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/__support/str_to_float.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {

LIBC_INLINE constexpr long double nanl(const char *arg) {
  auto result = internal::strtonan<long double>(arg);
  if (result.has_error())
    libc_errno = result.error;
  return result.value;
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_NANL_H
