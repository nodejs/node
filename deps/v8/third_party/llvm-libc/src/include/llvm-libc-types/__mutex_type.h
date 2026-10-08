//===-- Definition of a common mutex type ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES___MUTEX_TYPE_H
#define LLVM_LIBC_TYPES___MUTEX_TYPE_H

#include "__futex_word.h"
#include "pid_t.h"
#include "size_t.h"

typedef struct {
#ifdef __linux__
  __futex_word __ftxw;
#else
#error "Mutex type not defined for the target platform."
#endif

  unsigned int __priority_inherit : 1;
  unsigned int __recursive : 1;
  unsigned int __robust : 1;
  unsigned int __pshared : 1;
  unsigned int __error_checking : 1;

  pid_t __owner;
  size_t __lock_count;
} __mutex_type;

#endif // LLVM_LIBC_TYPES___MUTEX_TYPE_H
