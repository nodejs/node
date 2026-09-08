//===-- Definition of Elf64_Shdr type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_SHDR_H
#define LLVM_LIBC_TYPES_ELF64_SHDR_H

#include "Elf64_Addr.h"
#include "Elf64_Off.h"
#include "Elf64_Word.h"
#include "Elf64_Xword.h"

typedef struct {
  Elf64_Word sh_name;
  Elf64_Word sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr sh_addr;
  Elf64_Off sh_offset;
  Elf64_Xword sh_size;
  Elf64_Word sh_link;
  Elf64_Word sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
} Elf64_Shdr;

#endif // LLVM_LIBC_TYPES_ELF64_SHDR_H
