//===-- Definition of mbstate_t type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_MBSTATE_T_H
#define LLVM_LIBC_TYPES_MBSTATE_T_H

#include "../llvm-libc-macros/stdint-macros.h"

typedef struct {
  uint32_t __field1;
  uint8_t __field2;
  uint8_t __field3;
} mbstate_t;

#endif // LLVM_LIBC_TYPES_MBSTATE_T_H
