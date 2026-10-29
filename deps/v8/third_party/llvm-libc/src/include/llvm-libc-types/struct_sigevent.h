//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct sigevent type.
/// https://man7.org/linux/man-pages/man3/sigevent.3type.html
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SIGEVENT_H
#define LLVM_LIBC_TYPES_STRUCT_SIGEVENT_H

#include "pid_t.h"
#include "pthread_attr_t.h"
#include "union_sigval.h"

struct sigevent {
  int sigev_notify;
  int sigev_signo;
  union sigval sigev_value;
  void (*sigev_notify_function)(union sigval);
  pthread_attr_t *sigev_notify_attributes;
#ifdef __linux__
  pid_t sigev_notify_thread_id;
#endif
};

// Self-define for compatibility with code assuming a macro.
#define sigev_notify_thread_id sigev_notify_thread_id

#endif // LLVM_LIBC_TYPES_STRUCT_SIGEVENT_H
