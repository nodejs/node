//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Macros for POSIX regex.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_REGEX_MACROS_H
#define LLVM_LIBC_MACROS_REGEX_MACROS_H

// regcomp cflags
#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NOSUB 4
#define REG_NEWLINE 8

// regexec eflags
#define REG_NOTBOL 1
#define REG_NOTEOL 2

// Error codes
#define REG_NOMATCH 1
#define REG_BADPAT 2
#define REG_ECOLLATE 3
#define REG_ECTYPE 4
#define REG_EESCAPE 5
#define REG_ESUBREG 6
#define REG_EBRACK 7
#define REG_EPAREN 8
#define REG_EBRACE 9
#define REG_BADBR 10
#define REG_ERANGE 11
#define REG_ESPACE 12
#define REG_BADRPT 13

#endif // LLVM_LIBC_MACROS_REGEX_MACROS_H
