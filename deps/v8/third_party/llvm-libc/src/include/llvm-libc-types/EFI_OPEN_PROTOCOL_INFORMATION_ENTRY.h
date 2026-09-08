//===-- Definition of EFI_OPEN_PROTOCOL_INFORMATION_ENTRY type ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_OPEN_PROTOCOL_INFORMATION_ENTRY_H
#define LLVM_LIBC_TYPES_EFI_OPEN_PROTOCOL_INFORMATION_ENTRY_H

#include "../llvm-libc-macros/stdint-macros.h"
#include "EFI_HANDLE.h"

typedef struct {
  EFI_HANDLE AgentHandle;
  EFI_HANDLE ControllerHandle;
  uint32_t Attributes;
  uint32_t OpenCount;
} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;

#endif // LLVM_LIBC_TYPES_EFI_OPEN_PROTOCOL_INFORMATION_ENTRY_H
