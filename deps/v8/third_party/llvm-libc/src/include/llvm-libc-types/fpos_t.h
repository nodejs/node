//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of fpos_t type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_FPOS_T_H
#define LLVM_LIBC_TYPES_FPOS_T_H

#include "mbstate_t.h"
#include "off_t.h"

typedef struct {
  off_t __pos;
  mbstate_t __state;
} fpos_t;

#endif // LLVM_LIBC_TYPES_FPOS_T_H
