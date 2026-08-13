//===-- Definition of macros to be used with malloc functions -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_MALLOC_MACROS_H
#define LLVM_LIBC_MACROS_MALLOC_MACROS_H

// Note: these values only make sense when Scudo is used as the memory
// allocator.
#define M_PURGE (-101)
#define M_PURGE_ALL (-104)

#endif // LLVM_LIBC_MACROS_MALLOC_MACROS_H
