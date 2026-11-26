//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct iphdr.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IPHDR_H
#define LLVM_LIBC_TYPES_STRUCT_IPHDR_H

#include "../llvm-libc-macros/stdint-macros.h"

struct iphdr {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  unsigned int ihl : 4;
  unsigned int version : 4;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  unsigned int version : 4;
  unsigned int ihl : 4;
#else
#error "Unsupported byte order"
#endif
  uint8_t tos;
  uint16_t tot_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t check;
  uint32_t saddr;
  uint32_t daddr;
};

#endif // LLVM_LIBC_TYPES_STRUCT_IPHDR_H
