//===-- Definition of __barrier_type type ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES__BARRIER_TYPE_H
#define LLVM_LIBC_TYPES__BARRIER_TYPE_H

#include <stdbool.h>

typedef struct __attribute__((aligned(8 /* alignof (Barrier) */))) {
  unsigned expected;
  unsigned waiting;
  bool blocking;
  char entering[24 /* sizeof (CndVar) */];
  char exiting[24 /* sizeof (CndVar) */];
  char mutex[24 /* sizeof (Mutex) */];
} __barrier_type;

#endif // LLVM_LIBC_TYPES__BARRIER_TYPE_H
