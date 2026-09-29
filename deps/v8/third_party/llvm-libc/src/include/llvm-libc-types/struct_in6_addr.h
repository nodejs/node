//===-- Definition of struct in6_addr -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IN6_ADDR_H
#define LLVM_LIBC_TYPES_STRUCT_IN6_ADDR_H

#include "../llvm-libc-macros/stdint-macros.h"

struct in6_addr {
  __extension__ union {
    uint8_t s6_addr[16];
    uint16_t s6_addr16[8];
    uint32_t s6_addr32[4];
  };
};

#endif // LLVM_LIBC_TYPES_STRUCT_IN6_ADDR_H
