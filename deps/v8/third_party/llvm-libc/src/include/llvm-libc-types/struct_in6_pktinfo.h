//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct in6_pktinfo.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IN6_PKTINFO_H
#define LLVM_LIBC_TYPES_STRUCT_IN6_PKTINFO_H

#include "struct_in6_addr.h"

struct in6_pktinfo {
  struct in6_addr ipi6_addr;
  unsigned int ipi6_ifindex;
};

#endif // LLVM_LIBC_TYPES_STRUCT_IN6_PKTINFO_H
