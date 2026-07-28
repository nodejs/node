//===-- Definition of pthread_attr_t type ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_PTHREAD_ATTR_T_H
#define LLVM_LIBC_TYPES_PTHREAD_ATTR_T_H

#include "size_t.h"

typedef struct {
  int __detachstate;
  void *__stack;
  size_t __stacksize;
  size_t __guardsize;
} pthread_attr_t;

#endif // LLVM_LIBC_TYPES_PTHREAD_ATTR_T_H
