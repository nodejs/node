//===-- Definition of type imaxdiv_t --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __LLVM_LIBC_TYPES_IMAXDIV_T_H__
#define __LLVM_LIBC_TYPES_IMAXDIV_T_H__

#include <stdint.h>

typedef struct {
  intmax_t quot;
  intmax_t rem;
} imaxdiv_t;

#endif // __LLVM_LIBC_TYPES_IMAXDIV_T_H__
