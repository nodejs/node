//===-- Definition of type constraint_handler_t ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_INCLUDE_LLVM_LIBC_TYPES_CONSTRAINT_HANDLER_T_H
#define LLVM_LIBC_INCLUDE_LLVM_LIBC_TYPES_CONSTRAINT_HANDLER_T_H

#include "../llvm-libc-macros/annex-k-macros.h"
#include "errno_t.h"

#ifdef LIBC_HAS_ANNEX_K

typedef void (*constraint_handler_t)(const char *__restrict, void *__restrict,
                                     errno_t);

#endif // LIBC_HAS_ANNEX_K

#endif // LLVM_LIBC_INCLUDE_LLVM_LIBC_TYPES_CONSTRAINT_HANDLER_T_H
