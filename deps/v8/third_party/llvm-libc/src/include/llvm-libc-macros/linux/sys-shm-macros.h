//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux specific declarations of macros from sys/shm.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_SHM_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_SHM_MACROS_H

#include "../../__llvm-libc-common.h"
#include "../stdint-macros.h"

// shmget permission flags
#define SHM_R 0400
#define SHM_W 0200

// shmat flags
#define SHM_RDONLY 010000
#define SHM_RND 020000
#define SHM_REMAP 040000
#define SHM_EXEC 0100000

#define SHMLBA 4096
#define SHM_FAILED                                                             \
  __LLVM_LIBC_CAST(reinterpret_cast, void *,                                   \
                   __LLVM_LIBC_CAST(static_cast, intptr_t, -1))

// shmctl commands
#define SHM_LOCK 11
#define SHM_UNLOCK 12
#define SHM_STAT 13
#define SHM_INFO 14
#define SHM_STAT_ANY 15

// shm_mode upper byte flags in ipc_perm.
#define SHM_DEST 01000
#define SHM_LOCKED 02000

// shmflg bits for shmget
#define SHM_HUGETLB 04000
#define SHM_NORESERVE 010000

// Huge page size encoding when SHM_HUGETLB is set.
#define SHM_HUGE_SHIFT 26
#define SHM_HUGE_MASK 0x3f
#define SHM_HUGE_16KB (14U << SHM_HUGE_SHIFT)
#define SHM_HUGE_64KB (16U << SHM_HUGE_SHIFT)
#define SHM_HUGE_512KB (19U << SHM_HUGE_SHIFT)
#define SHM_HUGE_1MB (20U << SHM_HUGE_SHIFT)
#define SHM_HUGE_2MB (21U << SHM_HUGE_SHIFT)
#define SHM_HUGE_8MB (23U << SHM_HUGE_SHIFT)
#define SHM_HUGE_16MB (24U << SHM_HUGE_SHIFT)
#define SHM_HUGE_32MB (25U << SHM_HUGE_SHIFT)
#define SHM_HUGE_256MB (28U << SHM_HUGE_SHIFT)
#define SHM_HUGE_512MB (29U << SHM_HUGE_SHIFT)
#define SHM_HUGE_1GB (30U << SHM_HUGE_SHIFT)
#define SHM_HUGE_2GB (31U << SHM_HUGE_SHIFT)
#define SHM_HUGE_16GB (34U << SHM_HUGE_SHIFT)

#endif // LLVM_LIBC_MACROS_LINUX_SYS_SHM_MACROS_H
