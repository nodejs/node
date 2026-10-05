//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Policy class wrapper for network-related system calls.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_SYSCALL_POLICY_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_SYSCALL_POLICY_H

#include "hdr/types/socklen_t.h"
#include "hdr/types/ssize_t.h"
#include "hdr/types/struct_sockaddr.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/recvfrom.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/sendto.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/socket.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace net {

struct DefaultNetworkSyscallPolicy {
  LIBC_INLINE static ErrorOr<int> socket(int domain, int type, int protocol) {
    return linux_syscalls::socket(domain, type, protocol);
  }

  LIBC_INLINE static ErrorOr<ssize_t> sendto(int fd, const void *buf,
                                             size_t len, int flags,
                                             const struct sockaddr *dest_addr,
                                             socklen_t addrlen) {
    return linux_syscalls::sendto(fd, buf, len, flags, dest_addr, addrlen);
  }

  LIBC_INLINE static ErrorOr<ssize_t> recvfrom(int fd, void *buf, size_t len,
                                               int flags,
                                               struct sockaddr *src_addr,
                                               socklen_t *addrlen) {
    return linux_syscalls::recvfrom(fd, buf, len, flags, src_addr, addrlen);
  }

  LIBC_INLINE static ErrorOr<int> close(int fd) {
    return linux_syscalls::close(fd);
  }
};

} // namespace net
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_NETWORK_SYSCALL_POLICY_H
