//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct sockaddr_in6. This is the sockaddr specialization for
/// AF_INET6 sockets, as defined by posix.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SOCKADDR_IN6_H
#define LLVM_LIBC_TYPES_STRUCT_SOCKADDR_IN6_H

#include "../llvm-libc-macros/stdint-macros.h"
#include "in_port_t.h"
#include "sa_family_t.h"
#include "struct_in6_addr.h"

struct __attribute__((may_alias)) sockaddr_in6 {
  sa_family_t sin6_family;
  in_port_t sin6_port;
  uint32_t sin6_flowinfo;
  struct in6_addr sin6_addr;
  uint32_t sin6_scope_id;
};

#endif // LLVM_LIBC_TYPES_STRUCT_SOCKADDR_IN6_H
