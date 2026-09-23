//===-- Definition of Elf64_Nhdr type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_NHDR_H
#define LLVM_LIBC_TYPES_ELF64_NHDR_H

#include "Elf64_Word.h"

typedef struct {
  Elf64_Word n_namesz;
  Elf64_Word n_descsz;
  Elf64_Word n_type;
} Elf64_Nhdr;

#endif // LLVM_LIBC_TYPES_ELF64_NHDR_H
