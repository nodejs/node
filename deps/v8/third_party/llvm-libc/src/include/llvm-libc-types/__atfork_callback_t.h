//===-- Definition of type __atfork_callback_t ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES___ATFORK_CALLBACK_T_H
#define LLVM_LIBC_TYPES___ATFORK_CALLBACK_T_H

typedef void (*__atfork_callback_t)(void);

#endif // LLVM_LIBC_TYPES___ATFORK_CALLBACK_T_H
