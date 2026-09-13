//===-- Definition of pthread_mutexattr_t type ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_PTHREAD_MUTEXATTR_T_H
#define LLVM_LIBC_TYPES_PTHREAD_MUTEXATTR_T_H

// pthread_mutexattr_t is a collection bit mapped flags. The mapping is internal
// detail of the libc implementation.
typedef unsigned int pthread_mutexattr_t;

#endif // LLVM_LIBC_TYPES_PTHREAD_MUTEXATTR_T_H
