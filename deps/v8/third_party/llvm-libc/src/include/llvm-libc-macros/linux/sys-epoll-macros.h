//===-- Macros defined in sys/epoll.h header file -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_EPOLL_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_EPOLL_MACROS_H

#include "fcntl-macros.h"

// These are also defined in <linux/eventpoll.h> but that also contains a
// different definition of the epoll_event struct that is different from the
// userspace version.

#define EPOLL_CLOEXEC O_CLOEXEC

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN 0x1
#define EPOLLPRI 0x2
#define EPOLLOUT 0x4
#define EPOLLERR 0x8
#define EPOLLHUP 0x10
#define EPOLLRDNORM 0x40
#define EPOLLRDBAND 0x80
#define EPOLLWRNORM 0x100
#define EPOLLWRBAND 0x200
#define EPOLLMSG 0x400
#define EPOLLRDHUP 0x2000
#define EPOLLEXCLUSIVE 0x10000000
#define EPOLLWAKEUP 0x20000000
#define EPOLLONESHOT 0x40000000
#define EPOLLET 0x80000000

#endif // LLVM_LIBC_MACROS_LINUX_SYS_EPOLL_MACROS_H
