//===-- Definition of Elf32_Verneed type ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_VERNEED_H
#define LLVM_LIBC_TYPES_ELF32_VERNEED_H

#include "Elf32_Half.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Half vn_version;
  Elf32_Half vn_cnt;
  Elf32_Word vn_file;
  Elf32_Word vn_aux;
  Elf32_Word vn_next;
} Elf32_Verneed;

#endif // LLVM_LIBC_TYPES_ELF32_VERNEED_H
