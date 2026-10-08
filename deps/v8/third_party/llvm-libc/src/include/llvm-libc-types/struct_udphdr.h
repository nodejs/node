//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct udphdr. Fields are accessible using both linux-style
/// and BSD-style names.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_UDPHDR_H
#define LLVM_LIBC_TYPES_STRUCT_UDPHDR_H

#include "../llvm-libc-macros/stdint-macros.h"

struct udphdr {
  __extension__ union {
    struct {
      uint16_t uh_sport;
      uint16_t uh_dport;
      uint16_t uh_ulen;
      uint16_t uh_sum;
    };
    struct {
      uint16_t source;
      uint16_t dest;
      uint16_t len;
      uint16_t check;
    };
  };
};

#endif // LLVM_LIBC_TYPES_STRUCT_UDPHDR_H
