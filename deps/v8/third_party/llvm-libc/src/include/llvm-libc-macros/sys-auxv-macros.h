//===-- Macros defined in sys/auxv.h header file --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_SYS_AUXV_MACROS_H
#define LLVM_LIBC_MACROS_SYS_AUXV_MACROS_H

// Macros defining the aux vector indexes.
#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_NOTELF 10
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_PLATFORM 15
#define AT_HWCAP 16
#define AT_CLKTCK 17

#define AT_SECURE 23
#define AT_BASE_PLATFORM 24
#define AT_RANDOM 25
#define AT_HWCAP2 26
#define AT_HWCAP3 29
#define AT_HWCAP4 30

#define AT_EXECFN 31
#define AT_SYSINFO 32
#define AT_SYSINFO_EHDR 33

#ifndef AT_MINSIGSTKSZ
#define AT_MINSIGSTKSZ 51
#endif

#endif // LLVM_LIBC_MACROS_SYS_AUXV_MACROS_H
