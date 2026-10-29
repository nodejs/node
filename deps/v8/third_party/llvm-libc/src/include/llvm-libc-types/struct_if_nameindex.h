//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct if_nameindex.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IF_NAMEINDEX_H
#define LLVM_LIBC_TYPES_STRUCT_IF_NAMEINDEX_H

/// Structure storing a network interface index and its corresponding name.
struct if_nameindex {
  unsigned int if_index;
  char *if_name;
};

#endif // LLVM_LIBC_TYPES_STRUCT_IF_NAMEINDEX_H
