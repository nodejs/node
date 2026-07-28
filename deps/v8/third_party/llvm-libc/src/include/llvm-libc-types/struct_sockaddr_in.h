//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct sockaddr_in. This is the sockaddr specialization for
/// AF_INET sockets, as defined by posix.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SOCKADDR_IN_H
#define LLVM_LIBC_TYPES_STRUCT_SOCKADDR_IN_H

#include "in_port_t.h"
#include "sa_family_t.h"
#include "struct_in_addr.h"

struct __attribute__((may_alias)) sockaddr_in {
  sa_family_t sin_family; /* AF_INET */
  in_port_t sin_port;
  struct in_addr sin_addr;

  // For historic reasons, AF_INET addresses are 16 bytes. Users are not
  // expected to access the padding field, but it traditionally uses a
  // non-private name.
  char sin_zero[8];
};

#endif // LLVM_LIBC_TYPES_STRUCT_SOCKADDR_IN_H
