//===-- Definition of EFI_CAPSULE type ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_CAPSULE_H
#define LLVM_LIBC_TYPES_EFI_CAPSULE_H

#include "../llvm-libc-macros/stdint-macros.h"
#include "EFI_GUID.h"

typedef struct {
  EFI_GUID CapsuleGuid;
  uint32_t HeaderSize;
  uint32_t Flags;
  uint32_t CapsuleImageSize;
} EFI_CAPSULE_HEADER;

#define CAPSULE_FLAGS_PERSIST_ACROSS_RESET 0x00010000
#define CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE 0x00020000
#define CAPSULE_FLAGS_INITIATE_RESET 0x00040000

#endif // LLVM_LIBC_TYPES_EFI_CAPSULE_H
