//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct posix_dent.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_POSIX_DENT_H
#define LLVM_LIBC_TYPES_STRUCT_POSIX_DENT_H

#include "ino_t.h"
#include "off_t.h"
#include "reclen_t.h"

struct posix_dent {
  ino_t d_ino;
#ifdef __linux__
  off_t d_off;
#endif
  reclen_t d_reclen;
  unsigned char d_type;
  // The user code should use strlen to determine the actual size of d_name.
  // Likewise, it is incorrect and prohibited by the POSIX standard to determine
  // the size of struct posix_dent type using sizeof. The size should be got
  // using a different method, for example, from the d_reclen field.
  char d_name[1];
};

#endif // LLVM_LIBC_TYPES_STRUCT_POSIX_DENT_H
