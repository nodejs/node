//===-- Definition of Elf64_Verneed type ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_VERNEED_H
#define LLVM_LIBC_TYPES_ELF64_VERNEED_H

#include "Elf64_Half.h"
#include "Elf64_Word.h"

typedef struct {
  Elf64_Half vn_version;
  Elf64_Half vn_cnt;
  Elf64_Word vn_file;
  Elf64_Word vn_aux;
  Elf64_Word vn_next;
} Elf64_Verneed;

#endif // LLVM_LIBC_TYPES_ELF64_VERNEED_H
