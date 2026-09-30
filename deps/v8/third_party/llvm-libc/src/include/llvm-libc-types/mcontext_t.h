//===-- Definition of type mcontext_t -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_MCONTEXT_T_H
#define LLVM_LIBC_TYPES_MCONTEXT_T_H

#if defined(__x86_64__)
#include "x86_64/mcontext_t.h"
#else
#error "mcontext_t not available for your target architecture."
#endif

#endif // LLVM_LIBC_TYPES_MCONTEXT_T_H
