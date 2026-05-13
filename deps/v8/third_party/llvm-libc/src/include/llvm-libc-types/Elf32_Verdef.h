//===-- Definition of Elf32_Verdef type -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_VERDEF_H
#define LLVM_LIBC_TYPES_ELF32_VERDEF_H

#include "Elf32_Half.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Half vd_version;
  Elf32_Half vd_flags;
  Elf32_Half vd_ndx;
  Elf32_Half vd_cnt;
  Elf32_Word vd_hash;
  Elf32_Word vd_aux;
  Elf32_Word vd_next;
} Elf32_Verdef;

#endif // LLVM_LIBC_TYPES_ELF32_VERDEF_H
