//===-- Linux callonce fastpath -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_CALLONCE_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_CALLONCE_H

#include "src/__support/macros/config.h"
#include "src/__support/threads/linux/futex_utils.h"

namespace LIBC_NAMESPACE_DECL {
using CallOnceFlag = Futex;

namespace callonce_impl {
static constexpr FutexWordType NOT_CALLED = 0x0;
static constexpr FutexWordType START = 0x11;
static constexpr FutexWordType WAITING = 0x22;
static constexpr FutexWordType FINISH = 0x33;

// Avoid cmpxchg operation if the function has already been called.
// The destination operand of cmpxchg may receive a write cycle without
// regard to the result of the comparison.
LIBC_INLINE bool callonce_fastpath(CallOnceFlag *flag) {
  return flag->load(cpp::MemoryOrder::RELAXED) == FINISH;
}

template <class CallOnceCallback>
[[gnu::noinline, gnu::cold]] int callonce_slowpath(CallOnceFlag *flag,
                                                   CallOnceCallback callback) {

  auto *futex_word = reinterpret_cast<Futex *>(flag);

  FutexWordType not_called = NOT_CALLED;

  // The call_once call can return only after the called function |func|
  // returns. So, we use futexes to synchronize calls with the same flag value.
  if (futex_word->compare_exchange_strong(not_called, START)) {
    callback();
    auto status = futex_word->exchange(FINISH);
    if (status == WAITING)
      futex_word->notify_all();
    return 0;
  }

  FutexWordType status = START;
  if (futex_word->compare_exchange_strong(status, WAITING) || status == WAITING)
    futex_word->wait(WAITING);

  return 0;
}
} // namespace callonce_impl

} // namespace LIBC_NAMESPACE_DECL
#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_CALLONCE_H
