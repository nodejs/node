//===-- Definition of Elf32_auxv_t type -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_AUXV_T_H
#define LLVM_LIBC_TYPES_ELF32_AUXV_T_H

#include "../llvm-libc-macros/stdint-macros.h"

typedef struct {
  uint32_t a_type;
  union {
    uint32_t a_val;
  } a_un;
} Elf32_auxv_t;

#endif // LLVM_LIBC_TYPES_ELF32_AUXV_T_H
