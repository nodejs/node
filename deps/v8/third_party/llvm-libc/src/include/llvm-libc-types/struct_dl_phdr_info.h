//===-- Definition of type struct dl_phdr_info ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_DL_PHDR_INFO_H
#define LLVM_LIBC_TYPES_STRUCT_DL_PHDR_INFO_H

#include "../llvm-libc-macros/link-macros.h"
#include "size_t.h"
#include <stdint.h>

struct dl_phdr_info {
  ElfW(Addr) dlpi_addr;
  const char *dlpi_name;
  const ElfW(Phdr) * dlpi_phdr;
  ElfW(Half) dlpi_phnum;

  uint64_t dlpi_adds;
  uint64_t dlpi_subs;

  size_t dlpi_tls_modid;
  void *dlpi_tls_data;
};

#endif // LLVM_LIBC_TYPES_STRUCT_DL_PHDR_INFO_H
