//===-- Definition of Elf64_Vernaux type ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_VERNAUX_H
#define LLVM_LIBC_TYPES_ELF64_VERNAUX_H

#include "Elf64_Half.h"
#include "Elf64_Word.h"

typedef struct {
  Elf64_Word vna_hash;
  Elf64_Half vna_flags;
  Elf64_Half vna_other;
  Elf64_Word vna_name;
  Elf64_Word vna_next;
} Elf64_Vernaux;

#endif // NLLVM_LIBC_TYPES_ELF64_VERNAUX_H
