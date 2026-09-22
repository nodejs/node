//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of type struct statfs.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_STATFS_H
#define LLVM_LIBC_TYPES_STRUCT_STATFS_H

#include "fsblkcnt_t.h"
#include "fsfilcnt_t.h"
#include "fsid_t.h"

// NOTE: This structure may have different layouts on architectures we don't
// fully support (e.g. s390 or MIPS).

struct statfs {
  unsigned long f_type;
  unsigned long f_bsize;
  fsblkcnt_t f_blocks;
  fsblkcnt_t f_bfree;
  fsblkcnt_t f_bavail;
  fsfilcnt_t f_files;
  fsfilcnt_t f_ffree;
  fsid_t f_fsid;
  unsigned long f_namelen;
  unsigned long f_frsize;
  unsigned long f_flags;
  unsigned long f_spare[4];
};

#endif // LLVM_LIBC_TYPES_STRUCT_STATFS_H
