//===-- Definition of struct sockaddr -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SOCKADDR_H
#define LLVM_LIBC_TYPES_STRUCT_SOCKADDR_H

#include "sa_family_t.h"

struct __attribute__((may_alias)) sockaddr {
  sa_family_t sa_family;
  // sa_data is a variable length array. It is provided with a length of one
  // here as a placeholder.
  char sa_data[1];
};

#endif // LLVM_LIBC_TYPES_STRUCT_SOCKADDR_H
