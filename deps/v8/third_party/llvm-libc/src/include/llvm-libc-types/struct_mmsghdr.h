//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct mmsghdr.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_MMSGHDR_H
#define LLVM_LIBC_TYPES_STRUCT_MMSGHDR_H

#include "struct_msghdr.h"

struct mmsghdr {
  struct msghdr msg_hdr; /* Message header */
  unsigned int msg_len;  /* Number of bytes sent/received */
};

#endif // LLVM_LIBC_TYPES_STRUCT_MMSGHDR_H
