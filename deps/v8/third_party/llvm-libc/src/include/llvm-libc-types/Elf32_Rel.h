//===-- Definition of Elf32_Rel type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF32_REL_H
#define LLVM_LIBC_TYPES_ELF32_REL_H

#include "Elf32_Addr.h"
#include "Elf32_Word.h"

typedef struct {
  Elf32_Addr r_offset;
  Elf32_Word r_info;
} Elf32_Rel;

#endif // LLVM_LIBC_TYPES_ELF32_REL_H
