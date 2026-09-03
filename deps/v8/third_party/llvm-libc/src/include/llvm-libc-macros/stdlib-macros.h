//===-- Definition of macros to be used with stdlib functions ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_STDLIB_MACROS_H
#define LLVM_LIBC_MACROS_STDLIB_MACROS_H

#ifndef NULL
#define __need_NULL
#include <stddef.h>
#endif // NULL

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#ifndef MB_CUR_MAX
// We only support the "C" locale right now, so this is a constant byte.
#define MB_CUR_MAX 1
#endif // MB_CUR_MAX

#define RAND_MAX 2147483647

#endif // LLVM_LIBC_MACROS_STDLIB_MACROS_H
