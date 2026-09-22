//===-- Definition of EFI_TABLE_HEADER type -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_TABLE_HEADER_H
#define LLVM_LIBC_TYPES_EFI_TABLE_HEADER_H

#include "../llvm-libc-macros/stdint-macros.h"

typedef struct {
  uint64_t Signature;
  uint32_t Revision;
  uint32_t HeaderSize;
  uint32_t CRC32;
  uint32_t Reserved;
} EFI_TABLE_HEADER;

#endif // LLVM_LIBC_TYPES_EFI_TABLE_HEADER_H
