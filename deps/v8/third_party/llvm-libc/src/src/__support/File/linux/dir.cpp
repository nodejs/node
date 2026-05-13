//===--- Linux implementation of the Dir helpers --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/File/dir.h"

#include "src/__support/OSUtil/syscall.h" // For internal syscall function.
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include "hdr/fcntl_macros.h" // For open flags
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {

ErrorOr<int> platform_opendir(const char *name) {
  int open_flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;
#ifdef SYS_open
  int fd = LIBC_NAMESPACE::syscall_impl<int>(SYS_open, name, open_flags);
#elif defined(SYS_openat)
  int fd =
      LIBC_NAMESPACE::syscall_impl<int>(SYS_openat, AT_FDCWD, name, open_flags);
#else
#error                                                                         \
    "SYS_open and SYS_openat syscalls not available to perform an open operation."
#endif

  if (fd < 0) {
    return LIBC_NAMESPACE::Error(-fd);
  }
  return fd;
}

ErrorOr<size_t> platform_fetch_dirents(int fd, cpp::span<uint8_t> buffer) {
#ifdef SYS_getdents64
  long size = LIBC_NAMESPACE::syscall_impl<long>(SYS_getdents64, fd,
                                                 buffer.data(), buffer.size());
#else
#error "getdents64 syscalls not available to perform a fetch dirents operation."
#endif

  if (size < 0) {
    return LIBC_NAMESPACE::Error(static_cast<int>(-size));
  }
  return size;
}

int platform_closedir(int fd) {
  int ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_close, fd);
  if (ret < 0) {
    return static_cast<int>(-ret);
  }
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
