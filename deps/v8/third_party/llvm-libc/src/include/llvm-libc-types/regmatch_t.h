//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Type definition for regmatch_t.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_REGMATCH_T_H
#define LLVM_LIBC_TYPES_REGMATCH_T_H

#include "regoff_t.h"

typedef struct {
  regoff_t rm_so;
  regoff_t rm_eo;
} regmatch_t;

#endif // LLVM_LIBC_TYPES_REGMATCH_T_H
