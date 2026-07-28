//===-- Definition of Elf64_Sym type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_SYM_H
#define LLVM_LIBC_TYPES_ELF64_SYM_H

#include "Elf64_Addr.h"
#include "Elf64_Half.h"
#include "Elf64_Word.h"
#include "Elf64_Xword.h"

typedef struct {
  Elf64_Word st_name;
  unsigned char st_info;
  unsigned char st_other;
  Elf64_Half st_shndx;
  Elf64_Addr st_value;
  Elf64_Xword st_size;
} Elf64_Sym;

#endif // LLVM_LIBC_TYPES_ELF64_SYM_H
