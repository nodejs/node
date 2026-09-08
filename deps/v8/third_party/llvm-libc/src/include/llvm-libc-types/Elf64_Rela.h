//===-- Definition of Elf64_Rela type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_RELA_H
#define LLVM_LIBC_TYPES_ELF64_RELA_H

#include "Elf64_Addr.h"
#include "Elf64_Sxword.h"
#include "Elf64_Xword.h"

typedef struct {
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
  Elf64_Sxword r_addend;
} Elf64_Rela;

#endif // LLVM_LIBC_TYPES_ELF64_RELA_H
