//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ip_mreqn.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IP_MREQN_H
#define LLVM_LIBC_TYPES_STRUCT_IP_MREQN_H

#include "struct_in_addr.h"

struct ip_mreqn {
  struct in_addr imr_multiaddr;
  struct in_addr imr_address;
  int imr_ifindex;
};

#endif // LLVM_LIBC_TYPES_STRUCT_IP_MREQN_H
