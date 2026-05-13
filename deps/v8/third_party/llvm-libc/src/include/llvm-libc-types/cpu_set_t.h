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

typedef struct {
  // If a processor with more than 1024 CPUs is to be supported in future,
  // we need to adjust the size of this array.
  unsigned long __mask[128 / sizeof(unsigned long)];
} cpu_set_t;

#endif // LLVM_LIBC_TYPES_CPU_SET_T_H
