//===--- Implementation of a Unix RwLock class -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_UNIX_RWLOCK_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_UNIX_RWLOCK_H

#include "hdr/types/pid_t.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/common.h"
#include "src/__support/threads/identifier.h"
#include "src/__support/threads/raw_rwlock.h"

namespace LIBC_NAMESPACE_DECL {

class RwLock final {
  RawRwLock raw;
  cpp::Atomic<pid_t> writer_tid;

  LIBC_INLINE pid_t get_writer_tid() {
    return writer_tid.load(cpp::MemoryOrder::RELAXED);
  }

  LIBC_INLINE void set_writer_tid(pid_t tid) {
    writer_tid.store(tid, cpp::MemoryOrder::RELAXED);
  }

public:
  using LockResult = rwlock::LockResult;
  using Role = rwlock::Role;

  LIBC_INLINE constexpr RwLock(Role preference = Role::Reader,
                               bool is_pshared = false)
      : raw(preference, is_pshared), writer_tid(0) {}

  [[nodiscard]] LIBC_INLINE LockResult try_read_lock() {
    return raw.try_read_lock();
  }

  [[nodiscard]] LIBC_INLINE LockResult try_write_lock() {
    LockResult result = raw.try_write_lock();
    if (result == LockResult::Success)
      set_writer_tid(internal::gettid());
    return result;
  }

  [[nodiscard]]
  LIBC_INLINE LockResult
  read_lock(cpp::optional<Futex::Timeout> timeout = cpp::nullopt,
            unsigned spin_count = LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT) {
    if (get_writer_tid() == internal::gettid())
      return LockResult::Deadlock;
    return raw.read_lock(timeout, spin_count);
  }

  [[nodiscard]]
  LIBC_INLINE LockResult
  write_lock(cpp::optional<Futex::Timeout> timeout = cpp::nullopt,
             unsigned spin_count = LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT) {
    if (get_writer_tid() == internal::gettid())
      return LockResult::Deadlock;

    LockResult result = raw.write_lock(timeout, spin_count);
    if (result == LockResult::Success)
      set_writer_tid(internal::gettid());
    return result;
  }

  [[nodiscard]] LIBC_INLINE LockResult unlock() {
    bool is_writer_unlock = raw.has_active_writer();
    if (is_writer_unlock) {
      if (get_writer_tid() != internal::gettid())
        return LockResult::PermissionDenied;
      set_writer_tid(0);
    }
    return raw.unlock();
  }

  [[nodiscard]] LIBC_INLINE LockResult check_for_destroy() {
    return raw.check_for_destroy();
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_UNIX_RWLOCK_H
