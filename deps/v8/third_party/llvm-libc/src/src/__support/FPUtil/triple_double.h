//===-- Utilities for triple-double data type. ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_TRIPLE_DOUBLE_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_TRIPLE_DOUBLE_H

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

struct TripleDouble {
  double lo = 0.0;
  double mid = 0.0;
  double hi = 0.0;
};

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_TRIPLE_DOUBLE_H
