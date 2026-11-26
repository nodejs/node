//===-- Definition of the containerof macro -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_CONTAINEROF_MACRO_H
#define LLVM_LIBC_MACROS_CONTAINEROF_MACRO_H

#include "offsetof-macro.h"

// __extension__ silences -Wpedantic on the GNU statement expression. __ptr is
// const-volatile qualified so callers may pass pointers to const or volatile
// members while still type-checking against ((type *)0)->member; casting back
// via __UINTPTR_TYPE__ (rather than void *) avoids GCC -Wcast-qual when
// stripping those temporary qualifiers.
#define __containerof(ptr, type, member)                                       \
  (__extension__({                                                             \
    const volatile __typeof(((type *)0)->member) *__ptr = (ptr);               \
    (type *)(__UINTPTR_TYPE__)((const volatile char *)__ptr -                  \
                               offsetof(type, member));                        \
  }))

#endif // LLVM_LIBC_MACROS_CONTAINEROF_MACRO_H
