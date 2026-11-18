//===-- Definition of struct semid_ds -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SEMID_DS_H
#define LLVM_LIBC_TYPES_STRUCT_SEMID_DS_H

#include "struct_ipc_perm.h"
#include "time_t.h"

#if defined(__linux__)
struct semid_ds {
  struct ipc_perm sem_perm;
  time_t sem_otime;
#if defined(__x86_64__)
  // x86_64 kernel UAPI (asm/sembuf.h) includes extra padding fields after
  // sem_otime and sem_ctime, unlike asm-generic/sembuf.h used by other 64-bit
  // architectures (such as aarch64 and riscv64).
  unsigned long __unused1;
#endif
  time_t sem_ctime;
#if defined(__x86_64__)
  unsigned long __unused2;
#endif
  unsigned long sem_nsems;
  unsigned long __unused3;
  unsigned long __unused4;
};
#else
#error "semid_ds type not defined for the target platform."
#endif

#endif // LLVM_LIBC_TYPES_STRUCT_SEMID_DS_H
