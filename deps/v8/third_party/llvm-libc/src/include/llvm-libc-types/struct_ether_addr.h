//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ether_addr.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_ETHER_ADDR_H
#define LLVM_LIBC_TYPES_STRUCT_ETHER_ADDR_H

#include "../llvm-libc-macros/stdint-macros.h"

struct ether_addr {
  uint8_t ether_addr_octet[6];
};

#endif // LLVM_LIBC_TYPES_STRUCT_ETHER_ADDR_H
