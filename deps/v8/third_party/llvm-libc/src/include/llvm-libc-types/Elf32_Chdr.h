//===-- Definition of Elf32_Chdr type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_CHDR_H
#define LLVM_LIBC_TYPES_ELF32_CHDR_H

#include "Elf32_Word.h"

typedef struct {
  Elf32_Word ch_type;
  Elf32_Word ch_size;
  Elf32_Word ch_addralign;
} Elf32_Chdr;

#endif // LLVM_LIBC_TYPES_ELF32_CHDR_H
