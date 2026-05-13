//===-- Definition of EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL type ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_H
#define LLVM_LIBC_TYPES_EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_H

#include "../llvm-libc-macros/stdint-macros.h"
#include "EFI_STATUS.h"
#include "size_t.h"

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID                                   \
  {0x387477c2, 0x69c7, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS(EFIAPI *EFI_TEXT_RESET)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, bool ExtendedVerification);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_STRING)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const char16_t *String);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_TEST_STRING)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const char16_t *String);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_QUERY_MODE)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, size_t ModeNumber,
    size_t *Columns, size_t *Rows);

typedef EFI_STATUS(EFIAPI *EFI_TEXT_SET_MODE)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, size_t ModeNumber);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, size_t Attribute);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_SET_CURSOR_POSITION)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, size_t Column, size_t Row);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_ENABLE_CURSOR)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, bool Visible);

typedef struct {
  int32_t MaxMode;
  int32_t Mode;
  int32_t Attribute;
  int32_t CursorColumn;
  int32_t CursorRow;
  bool CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  EFI_TEXT_RESET Reset;
  EFI_TEXT_STRING OutputString;
  EFI_TEXT_TEST_STRING TestString;
  EFI_TEXT_QUERY_MODE QueryMode;
  EFI_TEXT_SET_MODE SetMode;
  EFI_TEXT_SET_ATTRIBUTE SetAttribute;
  EFI_TEXT_CLEAR_SCREEN ClearScreen;
  EFI_TEXT_SET_CURSOR_POSITION SetCursorPosition;
  EFI_TEXT_ENABLE_CURSOR EnableCursor;
  SIMPLE_TEXT_OUTPUT_MODE *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

#endif // LLVM_LIBC_TYPES_EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_H
