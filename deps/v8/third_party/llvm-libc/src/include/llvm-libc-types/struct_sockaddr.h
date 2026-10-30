//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct sockaddr.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SOCKADDR_H
#define LLVM_LIBC_TYPES_STRUCT_SOCKADDR_H

#include "sa_family_t.h"

struct __attribute__((may_alias)) sockaddr {
  sa_family_t sa_family;
  // A real address is longer than this and is passed as one of the
  // per-family structures instead. The length is what makes the whole
  // structure sixteen bytes, which is the size the kernel's own generic
  // address is and which code sizes its buffers against.
  char sa_data[14];
};

#endif // LLVM_LIBC_TYPES_STRUCT_SOCKADDR_H
