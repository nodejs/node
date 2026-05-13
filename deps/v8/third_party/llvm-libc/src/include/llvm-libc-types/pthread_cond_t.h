//===-- Definition of pthread_cond_t type ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_PTHREAD_COND_T_H
#define LLVM_LIBC_TYPES_PTHREAD_COND_T_H

#include "__futex_word.h"
#include "size_t.h"

typedef struct {
  union {
    void *__waiter_queue[2];
    size_t __waiter_size;
  };
  __futex_word __futex;
  char __is_shared;
  char __is_realtime;
  char __padding[2];
} pthread_cond_t;

#endif // LLVM_LIBC_TYPES_PTHREAD_COND_T_H
