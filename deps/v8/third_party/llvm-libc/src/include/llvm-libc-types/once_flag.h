//===-- Definition of once_flag type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_ONCE_FLAG_H
#define LLVM_LIBC_TYPES_ONCE_FLAG_H

#include "__futex_word.h"

#ifdef __linux__
typedef __futex_word once_flag;
#else
#error "Once flag type not defined for the target platform."
#endif

#endif // LLVM_LIBC_TYPES_ONCE_FLAG_H
