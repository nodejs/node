//===-- Definition of type cookie_io_functions_t --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_COOKIE_IO_FUNCTIONS_T_H
#define LLVM_LIBC_TYPES_COOKIE_IO_FUNCTIONS_T_H

#include "off64_t.h"
#include "size_t.h"
#include "ssize_t.h"

typedef ssize_t cookie_read_function_t(void *, char *, size_t);
typedef ssize_t cookie_write_function_t(void *, const char *, size_t);
typedef int cookie_seek_function_t(void *, off64_t *, int);
typedef int cookie_close_function_t(void *);

typedef struct {
  cookie_read_function_t *read;
  cookie_write_function_t *write;
  cookie_seek_function_t *seek;
  cookie_close_function_t *close;
} cookie_io_functions_t;

#endif // LLVM_LIBC_TYPES_COOKIE_IO_FUNCTIONS_T_H
