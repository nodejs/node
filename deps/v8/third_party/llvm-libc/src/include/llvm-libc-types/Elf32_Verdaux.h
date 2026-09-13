//===-- Definition of Elf32_Verdaux type ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_VERDAUX_H
#define LLVM_LIBC_TYPES_ELF32_VERDAUX_H

#include "Elf32_Word.h"

typedef struct {
  Elf32_Word vda_name;
  Elf32_Word vda_next;
} Elf32_Verdaux;

#endif // LLVM_LIBC_TYPES_ELF32_VERDAUX_H
