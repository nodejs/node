//===-- Definition of Elf32_Dyn type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_DYN_H
#define LLVM_LIBC_TYPES_ELF32_DYN_H

#include "Elf32_Addr.h"
#include "Elf32_Sword.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Sword d_tag;
  union {
    Elf32_Word d_val;
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;

#endif // LLVM_LIBC_TYPES_ELF32_DYN_H
