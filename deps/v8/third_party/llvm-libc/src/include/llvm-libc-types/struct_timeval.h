//===-- Definition of struct timeval -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_TIMEVAL_H
#define LLVM_LIBC_TYPES_STRUCT_TIMEVAL_H

#include "suseconds_t.h"
#include "time_t.h"

#if defined(__APPLE__)
// Darwin provides its own definition for struct timeval. Include it directly
// to ensure type compatibility and avoid redefinition errors.
#include <sys/_types/_timeval.h>
#else
struct timeval {
  time_t tv_sec;       // Seconds
  suseconds_t tv_usec; // Micro seconds
};
#endif // __APPLE__

#endif // LLVM_LIBC_TYPES_STRUCT_TIMEVAL_H
