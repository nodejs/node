//===-- Definition of macros to for extra dynamic linker functionality ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINK_MACROS_H
#define LLVM_LIBC_MACROS_LINK_MACROS_H

#include "../llvm-libc-types/Elf32_Addr.h"
#include "../llvm-libc-types/Elf32_Chdr.h"
#include "../llvm-libc-types/Elf32_Dyn.h"
#include "../llvm-libc-types/Elf32_Ehdr.h"
#include "../llvm-libc-types/Elf32_Half.h"
#include "../llvm-libc-types/Elf32_Lword.h"
#include "../llvm-libc-types/Elf32_Nhdr.h"
#include "../llvm-libc-types/Elf32_Off.h"
#include "../llvm-libc-types/Elf32_Phdr.h"
#include "../llvm-libc-types/Elf32_Rel.h"
#include "../llvm-libc-types/Elf32_Rela.h"
#include "../llvm-libc-types/Elf32_Shdr.h"
#include "../llvm-libc-types/Elf32_Sword.h"
#include "../llvm-libc-types/Elf32_Sym.h"
#include "../llvm-libc-types/Elf32_Word.h"
#include "../llvm-libc-types/Elf32_Xword.h"
#include "../llvm-libc-types/Elf32_auxv_t.h"
#include "../llvm-libc-types/Elf64_Addr.h"
#include "../llvm-libc-types/Elf64_Chdr.h"
#include "../llvm-libc-types/Elf64_Dyn.h"
#include "../llvm-libc-types/Elf64_Ehdr.h"
#include "../llvm-libc-types/Elf64_Half.h"
#include "../llvm-libc-types/Elf64_Lword.h"
#include "../llvm-libc-types/Elf64_Nhdr.h"
#include "../llvm-libc-types/Elf64_Off.h"
#include "../llvm-libc-types/Elf64_Phdr.h"
#include "../llvm-libc-types/Elf64_Rel.h"
#include "../llvm-libc-types/Elf64_Rela.h"
#include "../llvm-libc-types/Elf64_Shdr.h"
#include "../llvm-libc-types/Elf64_Sword.h"
#include "../llvm-libc-types/Elf64_Sxword.h"
#include "../llvm-libc-types/Elf64_Sym.h"
#include "../llvm-libc-types/Elf64_Word.h"
#include "../llvm-libc-types/Elf64_Xword.h"
#include "../llvm-libc-types/Elf64_auxv_t.h"

#ifdef __LP64__
#define ElfW(type) Elf64_##type
#else
#define ElfW(type) Elf32_##type
#endif

#endif
