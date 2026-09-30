//===-- Definition of cnd_t type ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_CND_T_H
#define LLVM_LIBC_TYPES_CND_T_H

#include "__futex_word.h"

typedef struct {
  void *__qfront;
  void *__qback;
  __futex_word __qmtx;
  char __padding[4];
} cnd_t;

#endif // LLVM_LIBC_TYPES_CND_T_H
