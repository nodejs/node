//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of type struct link_map.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_LINK_MAP_H
#define LLVM_LIBC_TYPES_STRUCT_LINK_MAP_H

#include "../llvm-libc-macros/link-macros.h"

// NB: This structure is accessed by debuggers and similar tools, which have
// their own definition of the structure. Changing the layout here will likely
// break those tools.
struct link_map {
  ElfW(Addr) l_addr;
  char *l_name;
  ElfW(Dyn) * l_ld;
  struct link_map *l_next, *l_prev;
};

#endif // LLVM_LIBC_TYPES_STRUCT_LINK_MAP_H
