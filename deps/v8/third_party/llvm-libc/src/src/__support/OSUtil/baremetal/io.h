//===---------- Baremetal implementation of IO utils ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_BAREMETAL_IO_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_BAREMETAL_IO_H

#include "include/llvm-libc-types/size_t.h"
#include "include/llvm-libc-types/ssize_t.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// These are intended to be provided by the vendor.
//
// The signature of these types and functions intentionally match `fopencookie`
// which allows the following:
//
// ```
// struct __llvm_libc_stdio_cookie { ... };
// ...
// struct __llvm_libc_stdio_cookie __llvm_libc_stdin_cookie;
// cookie_io_functions_t stdin_func = { .read = __llvm_libc_stdio_read };
// FILE *stdin = fopencookie(&__llvm_libc_stdin_cookie, "r", stdin_func);
// ...
// struct __llvm_libc_stdio_cookie __llvm_libc_stdout_cookie;
// cookie_io_functions_t stdout_func = { .write = __llvm_libc_stdio_write };
// FILE *stdout = fopencookie(&__llvm_libc_stdout_cookie, "w", stdout_func);
// ...
// struct __llvm_libc_stdio_cookie __llvm_libc_stderr_cookie;
// cookie_io_functions_t stderr_func = { .write = __llvm_libc_stdio_write };
// FILE *stderr = fopencookie(&__llvm_libc_stderr_cookie, "w", stderr_func);
// ```
//
// At the same time, implementation of functions like `printf` and `scanf` can
// use `__llvm_libc_stdio_read` and `__llvm_libc_stdio_write` directly to avoid
// the extra indirection.
//
// All three symbols `__llvm_libc_stdin_cookie`, `__llvm_libc_stdout_cookie`,
// and `__llvm_libc_stderr_cookie` must be provided, even if they don't point
// at anything.

struct __llvm_libc_stdio_cookie;

extern "C" ssize_t __llvm_libc_stdio_read(void *cookie, char *buf, size_t size);
extern "C" ssize_t __llvm_libc_stdio_write(void *cookie, const char *buf,
                                           size_t size);

void write_to_stderr(cpp::string_view msg);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_BAREMETAL_IO_H
