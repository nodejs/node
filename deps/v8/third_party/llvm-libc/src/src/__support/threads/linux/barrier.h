//===-- A platform independent abstraction layer for barriers --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC___SUPPORT_SRC_THREADS_LINUX_BARRIER_H
#define LLVM_LIBC___SUPPORT_SRC_THREADS_LINUX_BARRIER_H

#include "hdr/pthread_macros.h"
#include "include/llvm-libc-types/pthread_barrier_t.h"
#include "include/llvm-libc-types/pthread_barrierattr_t.h"
#include "src/__support/threads/CndVar.h"
#include "src/__support/threads/mutex.h"

namespace LIBC_NAMESPACE_DECL {

// NOTE: if the size of this class changes, you must ensure that the size of
// pthread_barrier_t (found in include/llvm-libc/types/pthread_barrier_t.h) is
// the same size
class Barrier {
private:
  unsigned expected;
  unsigned waiting;
  bool blocking;
  CndVar entering;
  CndVar exiting;
  Mutex m;

public:
  static int init(Barrier *b, const pthread_barrierattr_t *attr,
                  unsigned count);
  static int destroy(Barrier *b);
  int wait();
};

static_assert(sizeof(Barrier) <= sizeof(pthread_barrier_t),
              "The public pthread_barrier_t type cannot accommodate the "
              "internal barrier type.");

static_assert(alignof(Barrier) <= alignof(pthread_barrier_t),
              "The public pthread_barrier_t type has insufficient alignment "
              "for the internal barrier type.");

static_assert(sizeof(CndVar) <= 24,
              "CndVar size exceeds the size in __barrier_type.h");

static_assert(sizeof(Mutex) <= 24,
              "Mutex size exceeds the size in __barrier_type.h");

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC___SUPPORT_SRC_THREADS_LINUX_BARRIER_H
