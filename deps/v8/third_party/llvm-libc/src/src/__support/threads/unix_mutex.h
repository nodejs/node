//===--- Implementation of a Unix mutex class -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_UNIX_MUTEX_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_UNIX_MUTEX_H

#include "hdr/types/pid_t.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/threads/identifier.h"
#include "src/__support/threads/mutex_common.h"
#include "src/__support/threads/raw_mutex.h"

namespace LIBC_NAMESPACE_DECL {

// TODO: support shared/recursive/robust mutexes.
class Mutex final : private RawMutex {
  // Use bitfields to allow encoding more attributes.
  // TODO: the robustness and priority inheritance will need to be implemented.
  //       See also https://github.com/llvm/llvm-project/issues/194396
  LIBC_PREFERED_TYPE(bool) unsigned int priority_inherit : 1;
  LIBC_PREFERED_TYPE(bool) unsigned int recursive : 1;
  LIBC_PREFERED_TYPE(bool) unsigned int robust : 1;
  LIBC_PREFERED_TYPE(bool) unsigned int pshared : 1;
  LIBC_PREFERED_TYPE(bool) unsigned int error_checking : 1;

  // TLS address may not work across forked processes. Use thread id instead.
  cpp::Atomic<pid_t> owner;
  size_t lock_count;

  // CndVar needs to access Mutex as RawMutex
  friend class CndVar;

  template <class LockRoutine>
  LIBC_INLINE MutexError lock_impl(LockRoutine do_lock) {
    if (is_recursive() && owner == internal::gettid()) {
      if (LIBC_UNLIKELY(lock_count == cpp::numeric_limits<size_t>::max()))
        return MutexError::OVERFLOW;
      lock_count++;
      return MutexError::NONE;
    } else if (is_error_checking() && owner == internal::gettid())
      return MutexError::DEADLOCK;

    MutexError res = do_lock();

    if (res == MutexError::NONE) {
      if (is_recursive()) {
        owner = internal::gettid();
        lock_count = 1;
      } else if (is_error_checking()) {
        owner = internal::gettid();
      }
    }

    return res;
  }

public:
  LIBC_INLINE constexpr Mutex(bool is_priority_inherit, bool is_recursive,
                              bool is_robust, bool is_pshared,
                              bool is_error_checking = false)
      : RawMutex(), priority_inherit(is_priority_inherit),
        recursive(is_recursive), robust(is_robust), pshared(is_pshared),
        error_checking(is_error_checking), owner(0), lock_count(0) {}

  LIBC_INLINE static MutexError destroy(Mutex *lock) {
    LIBC_ASSERT(lock->owner == 0 && lock->lock_count == 0 &&
                "Mutex destroyed while being locked.");
    RawMutex::destroy(lock);
    return MutexError::NONE;
  }

  LIBC_INLINE MutexError lock() {
    return lock_impl([this] {
      // Since timeout is not specified, we do not need to check the return
      // value.
      // TODO: check deadlock? POSIX made it optional.
      this->RawMutex::lock(/* timeout=*/cpp::nullopt, this->pshared);
      return MutexError::NONE;
    });
  }

  LIBC_INLINE MutexError timed_lock(internal::AbsTimeout abs_time) {
    return lock_impl([this, abs_time] {
      // TODO: check deadlock? POSIX made it optional.
      if (this->RawMutex::lock(abs_time, this->pshared))
        return MutexError::NONE;
      return MutexError::TIMEOUT;
    });
  }

  LIBC_INLINE MutexError unlock() {
    if (is_recursive()) {
      // lock_count == 0 can happen if previous unlock is
      // suspended before signal frame
      if (owner != internal::gettid() || lock_count == 0)
        return MutexError::UNLOCK_WITHOUT_LOCK;

      lock_count--;
      if (lock_count == 0)
        owner = 0;
      else
        return MutexError::NONE;
    } else if (is_error_checking()) {
      if (owner != internal::gettid())
        return MutexError::UNLOCK_WITHOUT_LOCK;
      owner = 0;
    }
    if (this->RawMutex::unlock(this->pshared))
      return MutexError::NONE;
    return MutexError::UNLOCK_WITHOUT_LOCK;
  }

  LIBC_INLINE MutexError try_lock() {
    return lock_impl([this] {
      if (this->RawMutex::try_lock())
        return MutexError::NONE;
      return MutexError::BUSY;
    });
  }

  LIBC_INLINE bool can_be_requeued() const {
    return !this->pshared && !this->robust && !this->recursive &&
           !this->priority_inherit && !this->error_checking;
  }

  LIBC_INLINE bool is_robust() const { return this->robust; }
  LIBC_INLINE bool is_recursive() const { return this->recursive; }
  LIBC_INLINE bool is_error_checking() const { return this->error_checking; }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_UNIX_MUTEX_H
