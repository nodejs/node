//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Type definition for regex_t.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_REGEX_T_H
#define LLVM_LIBC_TYPES_REGEX_T_H

#include "size_t.h"

typedef struct {
  size_t re_nsub;
  void *__internal;
} regex_t;

#endif // LLVM_LIBC_TYPES_REGEX_T_H
