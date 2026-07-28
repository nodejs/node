//===-- Definition of type locale_t ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_LOCALE_T_H
#define LLVM_LIBC_TYPES_LOCALE_T_H

#define NUM_LOCALE_CATEGORIES 6

struct __locale_data;

struct __locale_t {
  struct __locale_data *data[NUM_LOCALE_CATEGORIES];
};

typedef struct __locale_t *locale_t;

#endif // LLVM_LIBC_TYPES_LOCALE_T_H
