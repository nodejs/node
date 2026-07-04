//===-- Definition of thrd_t type -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES___THREAD_TYPE_H
#define LLVM_LIBC_TYPES___THREAD_TYPE_H

typedef struct {
  void *__attrib;
} __thread_type;

#endif // LLVM_LIBC_TYPES___THREAD_TYPE_H
