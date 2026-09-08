//===-- Definition of thrd_start_t type -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_THRD_START_T_H
#define LLVM_LIBC_TYPES_THRD_START_T_H

typedef int (*thrd_start_t)(void *);

#endif // LLVM_LIBC_TYPES_THRD_START_T_H
