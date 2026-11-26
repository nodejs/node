//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux specific declarations of macros from sys/timerfd.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_TIMERFD_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_TIMERFD_MACROS_H

#include "fcntl-macros.h"

// Flags for timerfd_create.
#define TFD_CLOEXEC O_CLOEXEC
#define TFD_NONBLOCK O_NONBLOCK

// Flags for timerfd_settime.
#define TFD_TIMER_ABSTIME (1 << 0)
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)

#endif // LLVM_LIBC_MACROS_LINUX_SYS_TIMERFD_MACROS_H
