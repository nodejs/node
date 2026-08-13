//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of macros from sys/param.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_SYS_PARAM_MACROS_H
#define LLVM_LIBC_MACROS_SYS_PARAM_MACROS_H

#include "limits-macros.h"

// Number Bits per BYte
#ifndef NBBY
#ifdef __CHAR_BIT__
#define NBBY __CHAR_BIT__
#else
#define NBBY 8
#endif
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef howmany
#define howmany(n, d) (((n) + ((d) - 1)) / (d))
#endif

#ifndef roundup
#define roundup(n, d) (howmany(n, d) * (d))
#endif

#ifndef powerof2
#define powerof2(n) !(((n) - 1) & (n))
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#else
#define MAXPATHLEN 4096
#endif
#endif

#endif // LLVM_LIBC_MACROS_SYS_PARAM_MACROS_H
