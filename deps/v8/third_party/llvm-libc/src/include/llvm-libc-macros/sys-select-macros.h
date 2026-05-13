//===-- Macros defined in sys/select.h header file ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_SYS_SELECT_MACROS_H
#define LLVM_LIBC_MACROS_SYS_SELECT_MACROS_H

#define FD_SETSIZE 1024
#define __FD_SET_WORD_TYPE unsigned long
#define __FD_SET_WORD_SIZE (sizeof(__FD_SET_WORD_TYPE) * 8)
#define __FD_SET_ARRAYSIZE (FD_SETSIZE / __FD_SET_WORD_SIZE)

#define FD_ZERO(set)                                                           \
  do {                                                                         \
    unsigned i;                                                                \
    for (i = 0; i < __FD_SET_ARRAYSIZE; ++i)                                   \
      (set)->__set[i] = 0;                                                     \
  } while (0)

#define __FD_WORD(fd) ((fd) / __FD_SET_WORD_SIZE)
#define __FD_MASK(fd)                                                          \
  ((__FD_SET_WORD_TYPE)1) << ((__FD_SET_WORD_TYPE)((fd) % __FD_SET_WORD_SIZE))

#define FD_CLR(fd, set) (void)((set)->__set[__FD_WORD(fd)] &= ~__FD_MASK(fd))

#define FD_SET(fd, set) (void)((set)->__set[__FD_WORD(fd)] |= __FD_MASK(fd))

#define FD_ISSET(fd, set)                                                      \
  (int)(((set)->__set[__FD_WORD(fd)] & __FD_MASK(fd)) != 0)

#endif // LLVM_LIBC_MACROS_SYS_SELECT_MACROS_H
