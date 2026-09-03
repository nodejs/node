//===--- A self contained equivalent of std::mutex --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_MUTEX_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_MUTEX_H

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// Assume the calling thread has already obtained mutex ownership.
struct adopt_lock_t {
  explicit adopt_lock_t() = default;
};

// Tag used to make a scoped lock take ownership of a locked mutex.
constexpr adopt_lock_t adopt_lock{};

// An RAII class for easy locking and unlocking of mutexes.
template <typename MutexType> class lock_guard {
  MutexType &mutex;

public:
  // Calls `m.lock()` upon resource acquisition.
  explicit lock_guard(MutexType &m) : mutex(m) { mutex.lock(); }

  // Acquires ownership of the mutex object `m` without attempting to lock
  // it. The behavior is undefined if the current thread does not hold the
  // lock on `m`. Does not call `m.lock()` upon resource acquisition.
  lock_guard(MutexType &m, adopt_lock_t /* t */) : mutex(m) {}

  ~lock_guard() { mutex.unlock(); }

  // non-copyable
  lock_guard &operator=(const lock_guard &) = delete;
  lock_guard(const lock_guard &) = delete;
};

// Deduction guide for lock_guard to suppress CTAD warnings.
template <typename T> lock_guard(T &) -> lock_guard<T>;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_MUTEX_H
