//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of fsid_t type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_FSID_T_H
#define LLVM_LIBC_TYPES_FSID_T_H

typedef struct {
  int __val[2];
} fsid_t;

#endif // LLVM_LIBC_TYPES_FSID_T_H
