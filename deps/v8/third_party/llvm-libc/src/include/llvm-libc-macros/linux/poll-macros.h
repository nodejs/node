//===-- Macros defined in poll.h header file ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_POLL_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_POLL_MACROS_H

// From asm-generic/poll.h, redefined here to avoid redeclaring struct pollfd.
#ifndef POLLIN
#define POLLIN 0x0001
#endif

#ifndef POLLPRI
#define POLLPRI 0x0002
#endif

#ifndef POLLOUT
#define POLLOUT 0x0004
#endif

#ifndef POLLERR
#define POLLERR 0x0008
#endif

#ifndef POLLHUP
#define POLLHUP 0x0010
#endif

#ifndef POLLNVAL
#define POLLNVAL 0x0020
#endif

#ifndef POLLRDNORM
#define POLLRDNORM 0x0040
#endif

#ifndef POLLRDBAND
#define POLLRDBAND 0x0080
#endif

#ifndef POLLWRNORM
#define POLLWRNORM 0x0100
#endif

#ifndef POLLWRBAND
#define POLLWRBAND 0x0200
#endif

#ifndef POLLMSG
#define POLLMSG 0x0400
#endif

#ifndef POLLREMOVE
#define POLLREMOVE 0x1000
#endif

#ifndef POLLRDHUP
#define POLLRDHUP 0x2000
#endif

#endif // LLVM_LIBC_MACROS_LINUX_POLL_MACROS_H
