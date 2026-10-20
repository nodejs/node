//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal definitions for Linux kernel statx types, matching <linux/stat.h>.
///
/// We define equivalent `statx` and `statx_timestamp` types here instead
/// of directly including <linux/stat.h> to avoid name conflicts with system
/// libc implementations that choose provide their own `statx` definitions in
/// `<sys/stat.h>` (like musl or older glibc versions).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_STAT_KERNEL_STATX_TYPES_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_STAT_KERNEL_STATX_TYPES_H

#include "hdr/stdint_proxy.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace internal {

struct kernel_statx_timestamp {
  int64_t tv_sec;
  uint32_t tv_nsec;
  int32_t __reserved;
};

struct kernel_statx_buf {
  uint32_t stx_mask;       // What results were written
  uint32_t stx_blksize;    // Preferred general I/O size
  uint64_t stx_attributes; // Flags conveying information about the file
  uint32_t stx_nlink;      // Number of hard links
  uint32_t stx_uid;        // User ID of owner
  uint32_t stx_gid;        // Group ID of owner
  uint16_t stx_mode;       // File mode
  uint16_t __spare0[1];
  uint64_t stx_ino;             // Inode number
  uint64_t stx_size;            // File size
  uint64_t stx_blocks;          // Number of 512-byte blocks allocated
  uint64_t stx_attributes_mask; // Mask to show what's supported in
                                // stx_attributes
  struct kernel_statx_timestamp stx_atime; // Last access time
  struct kernel_statx_timestamp stx_btime; // File creation time
  struct kernel_statx_timestamp stx_ctime; // Last attribute change time
  struct kernel_statx_timestamp stx_mtime; // Last data modification time
  uint32_t stx_rdev_major;                 // Device ID of special file
  uint32_t stx_rdev_minor;
  uint32_t stx_dev_major; // ID of device containing file
  uint32_t stx_dev_minor;
  uint64_t stx_mnt_id;
  uint64_t __spare2;
  uint64_t __spare3[12]; // Spare space for future expansion
};

// The below mask value is based on the definition of a similarly
// named macro in linux/stat.h. When this flag is passed for the
// mask argument to the statx syscall, all fields except the
// stx_btime field will be filled in.
constexpr unsigned int KERNEL_STATX_BASIC_STATS_MASK = 0x7FF;

// This mask is used to check the type of the file descriptor.
constexpr unsigned int KERNEL_STATX_TYPE_MASK = 0x008;

} // namespace internal

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_STAT_KERNEL_STATX_TYPES_H
