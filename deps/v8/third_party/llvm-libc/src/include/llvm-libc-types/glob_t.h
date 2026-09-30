//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Type definition for glob_t.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_GLOB_T_H
#define LLVM_LIBC_TYPES_GLOB_T_H

#include "size_t.h"

typedef struct {
  size_t gl_pathc; // Count of paths matched by pattern.
  char **gl_pathv; // Pointer to a list of matched pathnames.
  size_t gl_offs;  // Slots to reserve at the beginning of gl_pathv.
} glob_t;

#endif // LLVM_LIBC_TYPES_GLOB_T_H
