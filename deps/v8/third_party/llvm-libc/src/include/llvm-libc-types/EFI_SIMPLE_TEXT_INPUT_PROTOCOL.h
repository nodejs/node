//===-- Definition of EFI_SIMPLE_TEXT_INPUT_PROTOCOL type -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_SIMPLE_TEXT_INPUT_PROTOCOL_H
#define LLVM_LIBC_TYPES_EFI_SIMPLE_TEXT_INPUT_PROTOCOL_H

#include "../llvm-libc-macros/EFIAPI-macros.h"
#include "../llvm-libc-macros/stdint-macros.h"
#include "EFI_EVENT.h"
#include "EFI_STATUS.h"
#include "char16_t.h"

#define EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID                                    \
  {0x387477c1, 0x69c7, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct {
  uint16_t ScanCode;
  char16_t UnicodeChar;
} EFI_INPUT_KEY;

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef EFI_STATUS(EFIAPI *EFI_INPUT_RESET)(
    struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, bool ExtendedVerification);
typedef EFI_STATUS(EFIAPI *EFI_INPUT_READ_KEY)(
    struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key);

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
  EFI_INPUT_RESET Reset;
  EFI_INPUT_READ_KEY ReadKeyStroke;
  EFI_EVENT WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

#endif // LLVM_LIBC_TYPES_EFI_SIMPLE_TEXT_INPUT_PROTOCOL_H
