//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux specific declarations of macros from sys/mount.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_MOUNT_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_MOUNT_MACROS_H

#define MS_RDONLY (1 << 0)
#define MS_NOSUID (1 << 1)
#define MS_NODEV (1 << 2)
#define MS_NOEXEC (1 << 3)
#define MS_SYNCHRONOUS (1 << 4)
#define MS_REMOUNT (1 << 5)
#define MS_MANDLOCK (1 << 6)
#define MS_DIRSYNC (1 << 7)
#define MS_NOSYMFOLLOW (1 << 8)
#define MS_NOATIME (1 << 10)
#define MS_NODIRATIME (1 << 11)
#define MS_BIND (1 << 12)
#define MS_MOVE (1 << 13)
#define MS_REC (1 << 14)
#define MS_SILENT (1 << 15)
#define MS_UNBINDABLE (1 << 17)
#define MS_PRIVATE (1 << 18)
#define MS_SLAVE (1 << 19)
#define MS_SHARED (1 << 20)
#define MS_RELATIME (1 << 21)
#define MS_STRICTATIME (1 << 24)
#define MS_LAZYTIME (1 << 25)

#endif // LLVM_LIBC_MACROS_LINUX_SYS_MOUNT_MACROS_H
