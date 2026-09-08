//===-- Definition of type struct dirent ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_DIRENT_H
#define LLVM_LIBC_TYPES_STRUCT_DIRENT_H

#include "ino_t.h"
#include "off_t.h"

struct dirent {
  ino_t d_ino;
#ifdef __linux__
  off_t d_off;
  unsigned short d_reclen;
#endif
  unsigned char d_type;
  // The user code should use strlen to determine actual the size of d_name.
  // Likewise, it is incorrect and prohibited by the POSIX standard to detemine
  // the size of struct dirent type using sizeof. The size should be got using
  // a different method, for example, from the d_reclen field on Linux.
  char d_name[1];
};

#endif // LLVM_LIBC_TYPES_STRUCT_DIRENT_H
