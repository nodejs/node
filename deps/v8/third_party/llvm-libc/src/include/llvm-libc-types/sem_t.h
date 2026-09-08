//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of the sem_t type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SEM_T_H
#define LLVM_LIBC_TYPES_SEM_T_H

#include "__futex_word.h"

typedef struct {
  // The semaphore count. Waiters block on this word.
  __futex_word __value;
  // Set to a fixed value by sem_init/sem_open and cleared by sem_destroy, so
  // that operations on an uninitialized or destroyed semaphore can be detected.
  unsigned int __canary;
  // Whether the semaphore is shared between processes.
  unsigned int __is_shared : 1;
} sem_t;

#endif // LLVM_LIBC_TYPES_SEM_T_H
