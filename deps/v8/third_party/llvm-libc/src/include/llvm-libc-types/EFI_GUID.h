//===-- Definition of EFI_GUID type -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_GUID_H
#define LLVM_LIBC_TYPES_EFI_GUID_H

#include "../llvm-libc-macros/stdint-macros.h"

typedef struct {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
} EFI_GUID;

#endif // LLVM_LIBC_TYPES_EFI_GUID_H
