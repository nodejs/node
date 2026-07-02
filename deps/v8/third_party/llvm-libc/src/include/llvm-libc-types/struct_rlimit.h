//===-- Definition of type struct rlimit ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_RLIMIT_H
#define LLVM_LIBC_TYPES_STRUCT_RLIMIT_H

#include "rlim_t.h"

struct rlimit {
  rlim_t rlim_cur;
  rlim_t rlim_max;
};

#endif // LLVM_LIBC_TYPES_STRUCT_RLIMIT_H
