//===-- Definition of struct msghdr ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_MSGHDR_H
#define LLVM_LIBC_TYPES_STRUCT_MSGHDR_H

#include "size_t.h"
#include "socklen_t.h"
#include "struct_iovec.h"

struct msghdr {
  void *msg_name;        /* Optional address */
  socklen_t msg_namelen; /* Size of address */
  struct iovec *msg_iov; /* Scatter/gather array */
  size_t msg_iovlen;     /* # elements in msg_iov */
  void *msg_control;     /* Ancillary data, see below */
  size_t msg_controllen; /* Ancillary data buffer len */
  int msg_flags;         /* Flags (unused) */
};

#endif // LLVM_LIBC_TYPES_STRUCT_MSGHDR_H
