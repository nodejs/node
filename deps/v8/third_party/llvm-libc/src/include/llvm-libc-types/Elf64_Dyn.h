//===-- Definition of Elf64_Dyn type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ELF64_DYN_H
#define LLVM_LIBC_TYPES_ELF64_DYN_H

#include "Elf64_Addr.h"
#include "Elf64_Off.h"
#include "Elf64_Sxword.h"
#include "Elf64_Xword.h"

typedef struct {
  Elf64_Sxword d_tag;
  union {
    Elf64_Xword d_val;
    Elf64_Addr d_ptr;
  } d_un;
} Elf64_Dyn;

#endif // LLVM_LIBC_TYPES_ELF64_DYN_H
