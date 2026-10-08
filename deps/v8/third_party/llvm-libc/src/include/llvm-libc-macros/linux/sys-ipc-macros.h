//===-- Definition of macros from sys/ipc.h -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_IPC_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_IPC_MACROS_H

#define IPC_PRIVATE 0

// Resource get request flags.
#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_NOWAIT 04000

// Control commands used with semctl, msgctl, and shmctl.
#define IPC_RMID 0
#define IPC_SET 1
#define IPC_STAT 2
#define IPC_INFO 3

#endif // LLVM_LIBC_MACROS_LINUX_SYS_IPC_MACROS_H
