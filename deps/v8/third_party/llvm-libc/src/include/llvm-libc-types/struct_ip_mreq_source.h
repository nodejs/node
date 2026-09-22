//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ip_mreq_source.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IP_MREQ_SOURCE_H
#define LLVM_LIBC_TYPES_STRUCT_IP_MREQ_SOURCE_H

#include "struct_in_addr.h"

struct ip_mreq_source {
  struct in_addr imr_multiaddr;
  struct in_addr imr_interface;
  struct in_addr imr_sourceaddr;
};

#endif // LLVM_LIBC_TYPES_STRUCT_IP_MREQ_SOURCE_H
