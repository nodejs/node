//===-- Definition of type rsize_t -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_RSIZE_T_H
#define LLVM_LIBC_TYPES_RSIZE_T_H

#include "../llvm-libc-macros/annex-k-macros.h"

#ifdef LIBC_HAS_ANNEX_K

#include "size_t.h"

typedef size_t rsize_t;

#endif // LIBC_HAS_ANNEX_K

#endif // LLVM_LIBC_TYPES_RSIZE_T_H
