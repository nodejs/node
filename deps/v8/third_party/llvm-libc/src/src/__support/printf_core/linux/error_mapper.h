//===-- Linux implementation of error mapper --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_LINUX_ERROR_MAPPER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_LINUX_ERROR_MAPPER_H

#include "hdr/errno_macros.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/error_mapper.h"

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

LIBC_INLINE static int internal_error_to_errno(int internal_error) {
  // System error occured, return error as is.
  if (internal_error < 1001 && internal_error > 0) {
    return internal_error;
  }

  // Map internal error to POSIX errnos.
  switch (-internal_error) {
  case WRITE_OK:
    return 0;
  case FILE_WRITE_ERROR:
    return EIO;
  case FILE_STATUS_ERROR:
    return EIO;
  case NULLPTR_WRITE_ERROR:
    return EINVAL;
  case INT_CONVERSION_ERROR:
    return ERANGE;
  case FIXED_POINT_CONVERSION_ERROR:
    return EINVAL;
  case ALLOCATION_ERROR:
    return ENOMEM;
  case OVERFLOW_ERROR:
    return EOVERFLOW;
  case ILLEGAL_WIDE_CHAR:
  case MB_CONVERSION_ERROR:
    return EILSEQ;
  default:
    LIBC_ASSERT(
        false &&
        "Invalid internal printf error code passed to internal_error_to_errno");
    return EINVAL;
  }
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_LINUX_ERROR_MAPPER_H
