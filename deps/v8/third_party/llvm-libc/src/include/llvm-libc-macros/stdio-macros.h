//===-- Macros defined in stdio.h header file -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_STDIO_MACROS_H
#define LLVM_LIBC_MACROS_STDIO_MACROS_H

#include "../llvm-libc-types/FILE.h"

#ifdef __cplusplus
extern "C" FILE *stdin;
extern "C" FILE *stdout;
extern "C" FILE *stderr;
#else
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
#endif

#ifndef stdin
#define stdin stdin
#endif

#ifndef stdout
#define stdout stdout
#endif

#ifndef stderr
#define stderr stderr
#endif

#ifndef EOF
#define EOF (-1)
#endif

#define BUFSIZ 1024

#define _IONBF 2
#define _IOLBF 1
#define _IOFBF 0

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

#endif // LLVM_LIBC_MACROS_STDIO_MACROS_H
