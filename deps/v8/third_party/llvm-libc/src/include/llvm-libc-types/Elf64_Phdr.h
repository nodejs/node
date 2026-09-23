//===-- Definition of Elf64_Phdr type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_PHDR_H
#define LLVM_LIBC_TYPES_ELF64_PHDR_H

#include "Elf64_Addr.h"
#include "Elf64_Off.h"
#include "Elf64_Word.h"
#include "Elf64_Xword.h"

typedef struct {
  Elf64_Word p_type;
  Elf64_Word p_flags;
  Elf64_Off p_offset;
  Elf64_Addr p_vaddr;
  Elf64_Addr p_paddr;
  Elf64_Xword p_filesz;
  Elf64_Xword p_memsz;
  Elf64_Xword p_align;
} Elf64_Phdr;

#endif // LLVM_LIBC_TYPES_ELF64_PHDR_H
