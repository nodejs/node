//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux implementation of the Dir helpers.
///
//===----------------------------------------------------------------------===//

#include "src/__support/File/dir.h"
#include "hdr/fcntl_macros.h"    // For open flags
#include "hdr/sys_stat_macros.h" // For S_ISDIR
#include "src/__support/OSUtil/linux/stat/kernel_statx_types.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/fcntl.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/open.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/statx.h"
#include "src/__support/OSUtil/syscall.h" // For internal syscall function.
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

ErrorOr<int> platform_opendir(const char *name) {
  return linux_syscalls::open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
}

ErrorOr<size_t> platform_fetch_dirents(int fd, cpp::span<uint8_t> buffer) {
#ifdef SYS_getdents64
  long size = LIBC_NAMESPACE::syscall_impl<long>(SYS_getdents64, fd,
                                                 buffer.data(), buffer.size());
#else
#error "getdents64 syscalls not available to perform a fetch dirents operation."
#endif

  if (size < 0)
    return LIBC_NAMESPACE::Error(static_cast<int>(-size));
  return size;
}

int platform_closedir(int fd) {
  auto ret = linux_syscalls::close(fd);
  if (!ret)
    return ret.error();
  return 0;
}

int platform_check_dir(int fd) {
  // 1. Verify fd is valid and open for reading.
  auto flags = linux_syscalls::fcntl(fd, F_GETFL);
  if (!flags)
    return flags.error();

  // Check if open for reading.
  if ((flags.value() & O_PATH) || ((flags.value() & O_ACCMODE) == O_WRONLY))
    return EBADF;

  // 2. Verify fd refers to a directory.
  internal::kernel_statx_buf xbuf;
  auto result = linux_syscalls::statx(fd, "", AT_EMPTY_PATH,
                                      internal::KERNEL_STATX_TYPE_MASK, &xbuf);
  if (!result)
    return result.error();

  if (!S_ISDIR(xbuf.stx_mode))
    return ENOTDIR;

  return 0;
}

size_t platform_dir_reclen(struct dirent *d) { return d->d_reclen; }

} // namespace LIBC_NAMESPACE_DECL
