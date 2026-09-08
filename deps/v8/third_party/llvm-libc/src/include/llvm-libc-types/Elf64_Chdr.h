//===-- Definition of Elf64_Chdr type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_CHDR_H
#define LLVM_LIBC_TYPES_ELF64_CHDR_H

#include "Elf64_Word.h"
#include "Elf64_Xword.h"

typedef struct {
  Elf64_Word ch_type;
  Elf64_Word ch_reserved;
  Elf64_Xword ch_size;
  Elf64_Xword ch_addralign;
} Elf64_Chdr;

#endif // LLVM_LIBC_TYPES_ELF64_CHDR_H
