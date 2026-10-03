//===--- Thread Identifier Header --------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_IDENTIFIER_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_IDENTIFIER_H

#ifdef LIBC_FULL_BUILD
#include "src/__support/threads/thread.h"
#endif // LIBC_FULL_BUILD

#include "hdr/types/pid_t.h"
#include "src/__support/OSUtil/syscall.h"
#include "src/__support/macros/optimization.h"
#include <sys/syscall.h>

namespace LIBC_NAMESPACE_DECL {
namespace internal {

LIBC_INLINE pid_t *get_tid_cache() {
#ifdef LIBC_FULL_BUILD
  return &self.attrib->tid;
#else
  // in non-full build mode, we do not control the fork routine. Therefore,
  // we do not cache tid at all.
  return nullptr;
#endif
}

LIBC_INLINE pid_t gettid() {
  pid_t *cache = get_tid_cache();
  if (LIBC_UNLIKELY(!cache || *cache <= 0))
    return syscall_impl<pid_t>(SYS_gettid);
  return *cache;
}

LIBC_INLINE void force_set_tid(pid_t tid) {
  pid_t *cache = get_tid_cache();
  if (cache)
    *cache = tid;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_IDENTIFIER_H
