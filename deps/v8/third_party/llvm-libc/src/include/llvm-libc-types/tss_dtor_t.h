//===-- Definition of the type tss_dtor_t ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_TSS_DTOR_T_H
#define LLVM_LIBC_TYPES_TSS_DTOR_T_H

typedef void (*tss_dtor_t)(void *);

#endif // LLVM_LIBC_TYPES_TSS_DTOR_T_H
