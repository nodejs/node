//===-- Definition of EFI_DEVICE_PATH_PROTOCOL type -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_DEVICE_PATH_PROTOCOL_H
#define LLVM_LIBC_TYPES_EFI_DEVICE_PATH_PROTOCOL_H

#include "../llvm-libc-macros/stdint-macros.h"

#define EFI_DEVICE_PATH_PROTOCOL_GUID                                          \
  {0x09576e91, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct _EFI_DEVICE_PATH_PROTOCOL {
  uint8_t Type;
  uint8_t SubType;
  uint8_t Length[2];
} EFI_DEVICE_PATH_PROTOCOL;

#endif // LLVM_LIBC_TYPES_EFI_DEVICE_PATH_PROTOCOL_H
