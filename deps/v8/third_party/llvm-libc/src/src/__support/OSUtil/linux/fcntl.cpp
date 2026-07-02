//===-- Implementation of internal fcntl ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/OSUtil/fcntl.h"

#include "hdr/errno_macros.h"
#include "hdr/fcntl_macros.h"
#include "hdr/types/mode_t.h"
#include "hdr/types/off_t.h"
#include "hdr/types/struct_f_owner_ex.h"
#include "hdr/types/struct_flock.h"
#include "hdr/types/struct_flock64.h"
#include "src/__support/OSUtil/syscall.h" // For internal syscall function.
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include <sys/syscall.h> // For syscall numbers.

namespace LIBC_NAMESPACE_DECL {
namespace internal {

ErrorOr<int> fcntl(int fd, int cmd, void *arg) {
#if SYS_fcntl
  constexpr auto FCNTL_SYSCALL_ID = SYS_fcntl;
#elif defined(SYS_fcntl64)
  constexpr auto FCNTL_SYSCALL_ID = SYS_fcntl64;
#else
#error "fcntl and fcntl64 syscalls not available."
#endif

  switch (cmd) {
  case F_OFD_SETLKW: {
    struct flock *flk = reinterpret_cast<struct flock *>(arg);
    // convert the struct to a flock64
    struct flock64 flk64;
    flk64.l_type = flk->l_type;
    flk64.l_whence = flk->l_whence;
    flk64.l_start = flk->l_start;
    flk64.l_len = flk->l_len;
    flk64.l_pid = flk->l_pid;
    // create a syscall
    int ret =
        LIBC_NAMESPACE::syscall_impl<int>(FCNTL_SYSCALL_ID, fd, cmd, &flk64);
    if (ret < 0)
      return Error(-ret);
    return ret;
  }
  case F_OFD_GETLK:
  case F_OFD_SETLK: {
    struct flock *flk = reinterpret_cast<struct flock *>(arg);
    // convert the struct to a flock64
    struct flock64 flk64;
    flk64.l_type = flk->l_type;
    flk64.l_whence = flk->l_whence;
    flk64.l_start = flk->l_start;
    flk64.l_len = flk->l_len;
    flk64.l_pid = flk->l_pid;
    // create a syscall
    int ret =
        LIBC_NAMESPACE::syscall_impl<int>(FCNTL_SYSCALL_ID, fd, cmd, &flk64);
    // On failure, return
    if (ret < 0)
      return Error(-ret);
    // Check for overflow, i.e. the offsets are not the same when cast
    // to off_t from off64_t.
    if (static_cast<off_t>(flk64.l_len) != flk64.l_len ||
        static_cast<off_t>(flk64.l_start) != flk64.l_start)
      return Error(EOVERFLOW);

    // Now copy back into flk, in case flk64 got modified
    flk->l_type = flk64.l_type;
    flk->l_whence = flk64.l_whence;
    flk->l_start = static_cast<decltype(flk->l_start)>(flk64.l_start);
    flk->l_len = static_cast<decltype(flk->l_len)>(flk64.l_len);
    flk->l_pid = flk64.l_pid;
    return ret;
  }
  case F_GETOWN: {
    struct f_owner_ex fex;
    int ret = LIBC_NAMESPACE::syscall_impl<int>(FCNTL_SYSCALL_ID, fd,
                                                F_GETOWN_EX, &fex);
    if (ret < 0)
      return Error(-ret);
    return fex.type == F_OWNER_PGRP ? -fex.pid : fex.pid;
  }
#ifdef SYS_fcntl64
  case F_GETLK: {
    if constexpr (FCNTL_SYSCALL_ID == SYS_fcntl64)
      cmd = F_GETLK64;
    break;
  }
  case F_SETLK: {
    if constexpr (FCNTL_SYSCALL_ID == SYS_fcntl64)
      cmd = F_SETLK64;
    break;
  }
  case F_SETLKW: {
    if constexpr (FCNTL_SYSCALL_ID == SYS_fcntl64)
      cmd = F_SETLKW64;
    break;
  }
#endif
  }

  // default, but may use rewritten cmd from above.
  int ret = LIBC_NAMESPACE::syscall_impl<int>(FCNTL_SYSCALL_ID, fd, cmd,
                                              reinterpret_cast<void *>(arg));
  if (ret < 0)
    return Error(-ret);
  return ret;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
