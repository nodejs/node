//===-- Definition of Elf32_Phdr type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_PHDR_H
#define LLVM_LIBC_TYPES_ELF32_PHDR_H

#include "Elf32_Addr.h"
#include "Elf32_Off.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;

#endif // LLVM_LIBC_TYPES_ELF32_PHDR_H
