//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation for `stat` functionality via the `statx` syscall.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_STAT_STAT_VIA_STATX_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_STAT_STAT_VIA_STATX_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/struct_stat.h"
#include "src/__support/OSUtil/linux/stat/kernel_statx_types.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/statx.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

// It is safe to include this kernel header as it is designed to be
// included from user programs without causing any name pollution.
#include <linux/kdev_t.h>

namespace LIBC_NAMESPACE_DECL {
namespace internal {

/// Populates `statbuf` via a call to the `statx` syscall.
LIBC_INLINE ErrorOr<int> stat_via_statx(int dirfd, const char *__restrict path,
                                        int flags,
                                        struct stat *__restrict statbuf) {
  kernel_statx_buf xbuf;
  ErrorOr<int> result = linux_syscalls::statx(
      dirfd, path, flags, KERNEL_STATX_BASIC_STATS_MASK, &xbuf);
  if (!result)
    return result;

  statbuf->st_dev = MKDEV(xbuf.stx_dev_major, xbuf.stx_dev_minor);
  statbuf->st_ino = static_cast<decltype(statbuf->st_ino)>(xbuf.stx_ino);
  statbuf->st_mode = xbuf.stx_mode;
  statbuf->st_nlink = xbuf.stx_nlink;
  statbuf->st_uid = xbuf.stx_uid;
  statbuf->st_gid = xbuf.stx_gid;
  statbuf->st_rdev = MKDEV(xbuf.stx_rdev_major, xbuf.stx_rdev_minor);
  statbuf->st_size = xbuf.stx_size;
  statbuf->st_atim.tv_sec = xbuf.stx_atime.tv_sec;
  statbuf->st_atim.tv_nsec = xbuf.stx_atime.tv_nsec;
  statbuf->st_mtim.tv_sec = xbuf.stx_mtime.tv_sec;
  statbuf->st_mtim.tv_nsec = xbuf.stx_mtime.tv_nsec;
  statbuf->st_ctim.tv_sec = xbuf.stx_ctime.tv_sec;
  statbuf->st_ctim.tv_nsec = xbuf.stx_ctime.tv_nsec;
  statbuf->st_blksize = xbuf.stx_blksize;
  statbuf->st_blocks =
      static_cast<decltype(statbuf->st_blocks)>(xbuf.stx_blocks);

  return 0;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_STAT_STAT_VIA_STATX_H
