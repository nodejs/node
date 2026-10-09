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

/*
 * L_tmpnam = 20 ("/tmp/" + 14 random chars + '\0').
 * Suffix length 14 (base-64) ensures a < 10^-12 collision
 * probability for up to 10^6 calls (via birthday bound).
 */
#ifndef L_tmpnam
#define L_tmpnam 20
#endif

/*
 * TMP_MAX = 1,000,000 (10^6 calls per process).
 * Generous decimal call ceiling for the L_tmpnam guarantee;
 * provides a 6x safety margin below the 6.2M limit (P ~= 2.6 x 10^-14).
 */
#ifndef TMP_MAX
#define TMP_MAX 1000000
#endif

// match linux/limits.h PATH_MAX
#ifndef FILENAME_MAX
#define FILENAME_MAX 4096
#endif

// Actually a lower bound on how many files we guarantee can be open at once.
// POSIX says "at least eight", which seems like a fine number to me.
#ifndef FOPEN_MAX
#define FOPEN_MAX 8
#endif

#endif // LLVM_LIBC_MACROS_STDIO_MACROS_H
