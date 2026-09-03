//===-- Definition of Elf32_Rela type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_RELA_H
#define LLVM_LIBC_TYPES_ELF32_RELA_H

#include "Elf32_Addr.h"
#include "Elf32_Sword.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Addr r_offset;
  Elf32_Word r_info;
  Elf32_Sword r_addend;
} Elf32_Rela;

#endif // LLVM_LIBC_TYPES_ELF32_RELA_H
