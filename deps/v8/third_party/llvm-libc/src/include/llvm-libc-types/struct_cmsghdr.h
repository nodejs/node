//===-- Definition of struct cmsghdr --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_CMSGHDR_H
#define LLVM_LIBC_TYPES_STRUCT_CMSGHDR_H

#include "size_t.h"

struct cmsghdr {
  // NB: This needs to match the kernel definition (which uses size_t), even
  // though POSIX says it should be socklen_t
  size_t cmsg_len; // data byte count, including header
  int cmsg_level;  // originating protocol
  int cmsg_type;   // protocol-specific type
};

#endif // LLVM_LIBC_TYPES_STRUCT_CMSGHDR_H
