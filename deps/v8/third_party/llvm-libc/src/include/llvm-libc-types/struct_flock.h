//===-- Definition of type struct flock64 ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_FLOCK_H
#define LLVM_LIBC_TYPES_STRUCT_FLOCK_H

#include "off_t.h"
#include "pid_t.h"

#include <stdint.h>

struct flock {
  int16_t l_type;
  int16_t l_whence;
  off_t l_start;
  off_t l_len;
  pid_t l_pid;
};

#endif // LLVM_LIBC_TYPES_STRUCT_FLOCK_H
