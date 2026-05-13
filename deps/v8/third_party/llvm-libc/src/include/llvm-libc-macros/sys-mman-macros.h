//===-- Macros defined in sys/mman.h header file --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_SYS_MMAN_MACROS_H
#define LLVM_LIBC_MACROS_SYS_MMAN_MACROS_H

// Use definitions from <linux/mman.h> to dispatch arch-specific flag values.
// For example, MCL_CURRENT/MCL_FUTURE/MCL_ONFAULT are different on different
// architectures.
#if __has_include(<linux/mman.h>)
#include <linux/mman.h>
#else
#error "cannot use <sys/mman.h> without proper system headers."
#endif

// Some posix standard flags may not be defined in system headers.
// Posix mmap flags.
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

// Posix memory advise flags. (posix_madvise)
#ifndef POSIX_MADV_NORMAL
#define POSIX_MADV_NORMAL MADV_NORMAL
#endif

#ifndef POSIX_MADV_SEQUENTIAL
#define POSIX_MADV_SEQUENTIAL MADV_SEQUENTIAL
#endif

#ifndef POSIX_MADV_RANDOM
#define POSIX_MADV_RANDOM MADV_RANDOM
#endif

#ifndef POSIX_MADV_WILLNEED
#define POSIX_MADV_WILLNEED MADV_WILLNEED
#endif

#ifndef POSIX_MADV_DONTNEED
#define POSIX_MADV_DONTNEED MADV_DONTNEED
#endif

#endif // LLVM_LIBC_MACROS_SYS_MMAN_MACROS_H
