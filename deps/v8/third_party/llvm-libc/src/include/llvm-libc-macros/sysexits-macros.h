//===-- Macros defined in sysexits.h header file --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef SYSEXITS_MACROS_H
#define SYSEXITS_MACROS_H

#define EX_OK 0           // Successful termination
#define EX_USAGE 64       // Command line usage error
#define EX_DATAERR 65     // Data format error
#define EX_NOINPUT 66     // Cannot open input
#define EX_NOUSER 67      // Addressee unknown
#define EX_NOHOST 68      // Host name unknown
#define EX_UNAVAILABLE 69 // Service unavailable
#define EX_SOFTWARE 70    // Internal software error
#define EX_OSERR 71       // Operating system error
#define EX_OSFILE 72      // System file error
#define EX_CANTCREAT 73   // Cannot create (user) output file
#define EX_IOERR 74       // Input/output error
#define EX_TEMPFAIL 75    // Temporary failure, try again
#define EX_PROTOCOL 76    // Remote protocol error
#define EX_NOPERM 77      // Permission denied
#define EX_CONFIG 78      // Configuration error

#endif // SYSEXITS_MACROS_H
