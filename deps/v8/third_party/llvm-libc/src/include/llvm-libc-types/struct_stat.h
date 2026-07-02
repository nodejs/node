//===-- Definition of struct stat -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_STAT_H
#define LLVM_LIBC_TYPES_STRUCT_STAT_H

#include "blkcnt_t.h"
#include "blksize_t.h"
#include "dev_t.h"
#include "gid_t.h"
#include "ino_t.h"
#include "mode_t.h"
#include "nlink_t.h"
#include "off_t.h"
#include "struct_timespec.h"
#include "uid_t.h"

struct stat {
  dev_t st_dev;
  ino_t st_ino;
  mode_t st_mode;
  nlink_t st_nlink;
  uid_t st_uid;
  gid_t st_gid;
  dev_t st_rdev;
  off_t st_size;
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
  blksize_t st_blksize;
  blkcnt_t st_blocks;
// Backwards compatibility macros for older kernel/standards
// that recorded timestamps in stat with one-second precision.
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
};

#endif // LLVM_LIBC_TYPES_STRUCT_STAT_H
