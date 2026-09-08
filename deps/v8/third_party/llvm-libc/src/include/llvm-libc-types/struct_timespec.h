//===-- Definition of struct timespec -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_TIMESPEC_H
#define LLVM_LIBC_TYPES_STRUCT_TIMESPEC_H

#if defined(__APPLE__)
// Darwin provides its own definition for struct timespec. Include it directly
// to ensure type compatibility and avoid redefinition errors.
#include <sys/_types/_timespec.h>
#else
#include "time_t.h"

struct timespec {
  time_t tv_sec; /* Seconds.  */
  /* Nanoseconds. Forced to 64-bit to match __kernel_timespec layout (C23
   * compliant). */
  __INT64_TYPE__ tv_nsec;
};
#endif // __APPLE__

#endif // LLVM_LIBC_TYPES_STRUCT_TIMESPEC_H
