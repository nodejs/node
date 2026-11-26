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

// The MS_* mount flags are defined by the kernel UAPI header.
#include <linux/mount.h>

// Flags for umount2. The kernel does not export these in a UAPI header.
#define MNT_FORCE 0x1
#define MNT_DETACH 0x2
#define MNT_EXPIRE 0x4
#define UMOUNT_NOFOLLOW 0x8

#endif // LLVM_LIBC_MACROS_LINUX_SYS_MOUNT_MACROS_H
