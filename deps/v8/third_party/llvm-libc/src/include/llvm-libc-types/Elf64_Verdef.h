//===-- Definition of Elf64_Verdef type -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_VERDEF_H
#define LLVM_LIBC_TYPES_ELF64_VERDEF_H

#include "Elf64_Half.h"
#include "Elf64_Word.h"

typedef struct {
  Elf64_Half vd_version;
  Elf64_Half vd_flags;
  Elf64_Half vd_ndx;
  Elf64_Half vd_cnt;
  Elf64_Word vd_hash;
  Elf64_Word vd_aux;
  Elf64_Word vd_next;
} Elf64_Verdef;

#endif // LLVM_LIBC_TYPES_ELF64_VERDEF_H
