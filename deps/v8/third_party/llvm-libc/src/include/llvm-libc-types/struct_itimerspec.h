//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct itimerspec.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_ITIMERSPEC_H
#define LLVM_LIBC_TYPES_STRUCT_ITIMERSPEC_H

#include "struct_timespec.h"

struct itimerspec {
  struct timespec it_interval;
  struct timespec it_value;
};

#endif // LLVM_LIBC_TYPES_STRUCT_ITIMERSPEC_H
