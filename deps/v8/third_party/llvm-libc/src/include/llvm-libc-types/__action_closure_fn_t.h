//===-- Definition of type __action_closure_fn_t --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES___ACTION_CLOSURE_FN_T_H
#define LLVM_LIBC_TYPES___ACTION_CLOSURE_FN_T_H

#include "VISIT.h"
#include "posix_tnode.h"

typedef void (*__action_closure_fn_t)(const posix_tnode *, VISIT, void *);

#endif // LLVM_LIBC_TYPES___ACTION_CLOSURE_FN_T_H
