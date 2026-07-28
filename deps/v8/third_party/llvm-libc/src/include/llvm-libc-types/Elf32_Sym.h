//===-- Definition of Elf32_Sym type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_SYM_H
#define LLVM_LIBC_TYPES_ELF32_SYM_H

#include "Elf32_Addr.h"
#include "Elf32_Half.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Word st_name;
  Elf32_Addr st_value;
  Elf32_Word st_size;
  unsigned char st_info;
  unsigned char st_other;
  Elf32_Half st_shndx;
} Elf32_Sym;

#endif // LLVM_LIBC_TYPES_ELF32_SYM_H
