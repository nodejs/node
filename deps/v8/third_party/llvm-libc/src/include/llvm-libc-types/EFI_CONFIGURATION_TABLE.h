//===-- Definition of EFI_CONFIGURATION_TABLE type ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_CONFIGURATION_TABLE_H
#define LLVM_LIBC_TYPES_EFI_CONFIGURATION_TABLE_H

#include "EFI_GUID.h"

typedef struct {
  EFI_GUID VendorGuid;
  void *VendorTable;
} EFI_CONFIGURATION_TABLE;

#endif // LLVM_LIBC_TYPES_EFI_CONFIGURATION_TABLE_H
