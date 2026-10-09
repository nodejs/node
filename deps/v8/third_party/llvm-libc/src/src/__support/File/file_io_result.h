//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Result type for file I/O operations.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_FILE_IO_RESULT_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_FILE_IO_RESULT_H

#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

struct FileIOResult {
  size_t value;
  int error;

  constexpr FileIOResult(size_t val) : value(val), error(0) {}
  constexpr FileIOResult(size_t val, int error) : value(val), error(error) {}

  constexpr bool has_error() { return error != 0; }

  constexpr operator size_t() { return value; }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_FILE_IO_RESULT_H
