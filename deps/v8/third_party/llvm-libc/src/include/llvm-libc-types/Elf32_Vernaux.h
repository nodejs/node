//===-- Definition of Elf32_Vernaux type ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_VERNAUX_H
#define LLVM_LIBC_TYPES_ELF32_VERNAUX_H

#include "Elf32_Half.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Word vna_hash;
  Elf32_Half vna_flags;
  Elf32_Half vna_other;
  Elf32_Word vna_name;
  Elf32_Word vna_next;
} Elf32_Vernaux;

#endif // NLLVM_LIBC_TYPES_ELF32_VERNAUX_H
