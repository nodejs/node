//===-- Definition of a cpu_set_t type ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_CPU_SET_T_H
#define LLVM_LIBC_TYPES_CPU_SET_T_H

#define __CPU_SETSIZE 1024
#define __NCPUBITS (8 * sizeof(unsigned long))
typedef unsigned long __cpu_set_mask_t;

typedef struct {
  __cpu_set_mask_t __mask[__CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

#endif // LLVM_LIBC_TYPES_CPU_SET_T_H
