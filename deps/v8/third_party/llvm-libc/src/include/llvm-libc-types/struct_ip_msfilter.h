//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ip_msfilter.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IP_MSFILTER_H
#define LLVM_LIBC_TYPES_STRUCT_IP_MSFILTER_H

#include "../llvm-libc-macros/stdint-macros.h"
#include "struct_in_addr.h"

struct ip_msfilter {
  struct in_addr imsf_multiaddr;
  struct in_addr imsf_interface;
  uint32_t imsf_fmode;
  uint32_t imsf_numsrc;
  struct in_addr imsf_slist[1]; // Variable size.
};

#endif // LLVM_LIBC_TYPES_STRUCT_IP_MSFILTER_H
