//===-- Definition of EFI_TIME type ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_TIME_H
#define LLVM_LIBC_TYPES_EFI_TIME_H

#include "../llvm-libc-macros/stdint-macros.h"

typedef struct {
  uint16_t Year;  // 1900 - 9999
  uint8_t Month;  // 1 - 12
  uint8_t Day;    // 1 - 31
  uint8_t Hour;   // 0 - 23
  uint8_t Minute; // 0 - 59
  uint8_t Second; // 0 - 59
  uint8_t Pad1;
  uint32_t Nanosecond; // 0 - 999,999,999
  int16_t TimeZone;    // --1440 to 1440 or 2047
} EFI_TIME;

#define EFI_TIME_ADJUST_DAYLIGHT 0x01
#define EFI_TIME_IN_DAYLIGHT 0x02

#define EFI_UNSPECIFIED_TIMEZONE 0x07FF

typedef struct {
  uint32_t Resolution;
  uint32_t Accuracy;
  bool SetsToZero;
} EFI_TIME_CAPABILITIES;

#endif // LLVM_LIBC_TYPES_EFI_TIME_H
