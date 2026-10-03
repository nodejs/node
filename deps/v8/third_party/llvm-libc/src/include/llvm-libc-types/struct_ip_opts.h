//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ip_opts.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IP_OPTS_H
#define LLVM_LIBC_TYPES_STRUCT_IP_OPTS_H

#include "struct_in_addr.h"

struct ip_opts {
  struct in_addr ip_dst;
  char ip_opts[40]; // Variable size.
};

#endif // LLVM_LIBC_TYPES_STRUCT_IP_OPTS_H
