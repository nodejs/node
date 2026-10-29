//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ipv6_mreq.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IPV6_MREQ_H
#define LLVM_LIBC_TYPES_STRUCT_IPV6_MREQ_H

#include "struct_in6_addr.h"

struct ipv6_mreq {
  struct in6_addr ipv6mr_multiaddr;
  unsigned int ipv6mr_interface;
};

#endif // LLVM_LIBC_TYPES_STRUCT_IPV6_MREQ_H
