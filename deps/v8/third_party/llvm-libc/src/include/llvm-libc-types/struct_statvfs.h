//===-- Definition of type struct statvfs ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_STATVFS_H
#define LLVM_LIBC_TYPES_STRUCT_STATVFS_H

#include "fsblkcnt_t.h"
#include "fsfilcnt_t.h"

struct statvfs {
  unsigned long f_bsize;   /* Filesystem block size */
  unsigned long f_frsize;  /* Fragment size */
  fsblkcnt_t f_blocks;     /* Size of fs in f_frsize units */
  fsblkcnt_t f_bfree;      /* Number of free blocks */
  fsblkcnt_t f_bavail;     /* Number of free blocks for unprivileged users */
  fsfilcnt_t f_files;      /* Number of inodes */
  fsfilcnt_t f_ffree;      /* Number of free inodes */
  fsfilcnt_t f_favail;     /* Number of free inodes for unprivileged users */
  unsigned long f_fsid;    /* Filesystem ID */
  unsigned long f_flag;    /* Mount flags */
  unsigned long f_namemax; /* Maximum filename length */
};

#endif // LLVM_LIBC_TYPES_STRUCT_STATVFS_H
