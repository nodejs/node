//===-- Definition of pthread_mutex_t type --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_PTHREAD_RWLOCK_T_H
#define LLVM_LIBC_TYPES_PTHREAD_RWLOCK_T_H

#include "__futex_word.h"
#include "pid_t.h"
typedef struct {
  struct {
    unsigned __is_pshared : 1;
    unsigned __preference : 1;
    int __state;
    __futex_word __wait_queue_mutex;
    __futex_word __pending_readers;
    __futex_word __pending_writers;
    __futex_word __reader_serialization;
    __futex_word __writer_serialization;
  } __raw;
  pid_t __writer_tid;
} pthread_rwlock_t;

#endif // LLVM_LIBC_TYPES_PTHREAD_RWLOCK_T_H
