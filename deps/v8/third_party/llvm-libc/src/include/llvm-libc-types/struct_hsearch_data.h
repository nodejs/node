//===-- Definition of type struct hsearch_data ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_HSEARCH_DATA_H
#define LLVM_LIBC_TYPES_STRUCT_HSEARCH_DATA_H

struct hsearch_data {
  void *__opaque;
  unsigned int __unused[2];
};

#endif // LLVM_LIBC_TYPES_STRUCT_HSEARCH_DATA_H
