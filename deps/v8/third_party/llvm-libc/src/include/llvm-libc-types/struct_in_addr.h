//===-- Definition of struct in_addr --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IN_ADDR_H
#define LLVM_LIBC_TYPES_STRUCT_IN_ADDR_H

#include "in_addr_t.h"

struct in_addr {
  in_addr_t s_addr;
};

#endif // LLVM_LIBC_TYPES_STRUCT_IN_ADDR_H
