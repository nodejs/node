// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_MUTEX_H_
#define V8_BASE_PLATFORM_MUTEX_H_

#include <optional>

#include "include/v8config.h"

#if V8_OS_DARWIN
#include <os/lock.h>

#include "absl/synchronization/mutex.h"
#endif

#if V8_OS_POSIX
#include <pthread.h>
#endif

#include "src/base/base-export.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"

#if V8_OS_WIN
#include "src/base/win32-headers.h"
#endif

#if V8_OS_STARBOARD
#include "starboard/common/mutex.h"
#include "starboard/common/recursive_mutex.h"
#include "starboard/common/rwlock.h"
#endif

namespace v8 {
namespace base {

class ConditionVariable;

// Mutex - a replacement for std::mutex
//
// This class is a synchronization primitive that can be used to protect shared
// data from being simultaneously accessed by multiple threads. A mutex offers
// exclusive, non-recursive ownership semantics:
// - A calling thread owns a mutex from the time that it successfully calls
//   either |Lock()| or |TryLock()| until it calls |Unlock()|.
// - When a thread owns a mutex, all other threads will block (for calls to
//   |Lock()|) or receive a |false| return value (for |TryLock()|) if they
//   attempt to claim ownership of the mutex.
// A calling thread must not own the mutex prior to calling |Lock()| or
// |TryLock()|. The behavior of a program is undefined if a mutex is destroyed
// while still owned by some thread. The Mutex class is non-copyable.

class V8_BASE_EXPORT Mutex final {
 public:
  Mutex();
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  ~Mutex();

  // Locks the given mutex. If the mutex is currently unlocked, it becomes
  // locked and owned by the calling thread, and immediately. If the mutex
  // is already locked by another thread, suspends the calling thread until
  // the mutex is unlocked.
  void Lock();

  // Unlocks the given mutex. The mutex is assumed to be locked and owned by
  // the calling thread on entrance.
  void Unlock();

  // Tries to lock the given mutex. Returns whether the mutex was
  // successfully locked.
  // Note: Instead of `DCHECK(!mutex.TryLock())` use `mutex.AssertHeld()`.
  bool TryLock() V8_WARN_UNUSED_RESULT;

  // The implementation-defined native handle type.
#if V8_OS_POSIX
  using NativeHandle = pthread_mutex_t;
#elif V8_OS_WIN
  using NativeHandle = V8_SRWLOCK;
#elif V8_OS_STARBOARD
  using NativeHandle = SbMutex;
#endif

  NativeHandle& native_handle() {
    return native_handle_;
  }
  const NativeHandle& native_handle() const {
    return native_handle_;
  }

  V8_INLINE void AssertHeld() const {
    // If this access results in a race condition being detected by TSan, this
    // means that you in fact did *not* hold the mutex.
    DCHECK_EQ(1, level_);
  }

 private:
#ifdef DEBUG
  // This is being used for Assert* methods. Accesses are only allowed if you
  // actually hold the mutex, otherwise you would get race conditions.
  int level_;
#endif

  V8_INLINE void AssertHeldAndUnmark() {
#ifdef DEBUG
    // If this access results in a race condition being detected by TSan, this
    // means that you in fact did *not* hold the mutex.
    DCHECK_EQ(1, level_);
    level_--;
#endif
  }

  V8_INLINE void AssertUnheldAndMark() {
#ifdef DEBUG
    // This is only invoked *after* actually getting the mutex, so should not
    // result in race conditions.
    DCHECK_EQ(0, level_);
    level_++;
#endif
  }

  friend class ConditionVariable;

  NativeHandle native_handle_;
};

/*
TODO(verwaest): Fix warnings so we can get verification
#if defined(__clang__)
#define V8_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
*/
#define V8_THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
/*
#endif
*/

// V8_LOCKABLE
//
// Documents if a class/type is a lockable type (such as the `Mutex` class).
#define V8_LOCKABLE V8_THREAD_ANNOTATION_ATTRIBUTE__(lockable)

// V8_EXCLUSIVE_LOCK_FUNCTION()
//
// Documents functions that acquire a lock in the body of a function, and do
// not release it.
#define V8_EXCLUSIVE_LOCK_FUNCTION(...) \
  V8_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

// V8_UNLOCK_FUNCTION()
//
// Documents functions that expect a lock to be held on entry to the function,
// and release it in the body of the function.
#define V8_UNLOCK_FUNCTION(...) \
  V8_THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

// V8_EXCLUSIVE_TRYLOCK_FUNCTION()
//
// Documents functions that try to acquire a lock, and return success or failure
// (or a non-boolean value that can be interpreted as a boolean).
// The first argument should be `true` for functions that return `true` on
// success, or `false` for functions that return `false` on success. The second
// argument specifies the mutex that is locked on success. If unspecified, this
// mutex is assumed to be `this`.
#define V8_EXCLUSIVE_TRYLOCK_FUNCTION(...) \
  V8_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))

// The behavior of this class depends on platform support:
// 1. When platform supports is available:
//
// Simple spinning lock. It will spin in user space a set number of times before
// going into the kernel to sleep.
//
// This is intended to give "the best of both worlds" between a SpinLock and
// base::Lock:
// - SpinLock: Inlined fast path, no external function calls, just
//   compare-and-swap. Short waits do not go into the kernel. Good behavior in
//   low contention cases.
// - base::Lock: Good behavior in case of contention.
//
// We don't rely on base::Lock which we could make spin (by calling TryLock() in
// a loop), as performance is below a custom spinlock as seen on high-level
// benchmarks. Instead this implements a simple non-recursive mutex on top of:
// - Linux   : futex()
// - Windows : SRWLock
// - MacOS   : os_unfair_lock
// - POSIX   : pthread_mutex_trylock()
//
// The main difference between this and a libc implementation is that it only
// supports the simplest path: private (to a process), non-recursive mutexes
// with no priority inheritance, no timed waits.
//
// As an interesting side-effect to be used in the allocator, this code does not
// make any allocations, locks are small with a constexpr constructor and no
// destructor.
//
// 2. Otherwise: This is a simple SpinLock, in the sense that it does not have
//    any awareness of other threads' behavior.
class V8_BASE_EXPORT V8_LOCKABLE SpinningMutex final {
 public:
  SpinningMutex();
  V8_INLINE void Lock() V8_EXCLUSIVE_LOCK_FUNCTION();
  void Unlock() V8_UNLOCK_FUNCTION();
  bool TryLock() V8_EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void AssertHeld() const {}  // Not supported.

 private:
  V8_NOINLINE void AcquireSpinThenBlock() V8_EXCLUSIVE_LOCK_FUNCTION();
  void LockSlow() V8_EXCLUSIVE_LOCK_FUNCTION();

  // See below, the latency of YIELD_PROCESSOR can be as high as ~150
  // cycles. Meanwhile, sleeping costs a few us. Spinning 64 times at 3GHz would
  // cost 150 * 64 / 3e9 ~= 3.2us.
  //
  // This applies to Linux kernels, on x86_64. On ARM we might want to spin
  // more.
  static constexpr int kSpinCount = 64;

#if V8_TARGET_OS_LINUX
  void FutexWait();
  void FutexWake();

  static constexpr int kUnlocked = 0;
  static constexpr int kLockedUncontended = 1;
  static constexpr int kLockedContended = 2;

  std::atomic<int32_t> state_{kUnlocked};
#elif V8_OS_WIN
  V8_SRWLOCK lock_;
#elif V8_OS_DARWIN
  os_unfair_lock lock_;
#elif V8_OS_POSIX
  pthread_mutex_t lock_;
#elif V8_OS_FUCHSIA
  sync_mutex lock_;
#else
  std::atomic<bool> lock_{false};
#endif
};

V8_INLINE void SpinningMutex::Lock() {
  // Not marked `[[likely]]`, as:
  // 1. We don't know how much contention the lock would experience
  // 2. This may lead to weird-looking code layout when inlined into a caller
  // with `[[(un)likely]]` attributes.
  if (TryLock()) {
    return;
  }

  return AcquireSpinThenBlock();
}

#if V8_TARGET_OS_LINUX

V8_INLINE bool SpinningMutex::TryLock() {
  // Using the weak variant of compare_exchange(), which may fail spuriously. On
  // some architectures such as ARM, CAS is typically performed as a LDREX/STREX
  // pair, where the store may fail. In the strong version, there is a loop
  // inserted by the compiler to retry in these cases.
  //
  // Since we are retrying in Lock() anyway, there is no point having two nested
  // loops.
  int expected = kUnlocked;
  return (state_.load(std::memory_order_relaxed) == expected) &&
         state_.compare_exchange_weak(expected, kLockedUncontended,
                                      std::memory_order_acquire,
                                      std::memory_order_relaxed);
}

V8_INLINE void SpinningMutex::Unlock() {
  if (state_.exchange(kUnlocked, std::memory_order_release) == kLockedContended)
      [[unlikely]] {
    // |kLockedContended|: there is a waiter to wake up.
    //
    // Here there is a window where the lock is unlocked, since we just set it
    // to |kUnlocked| above. Meaning that another thread can grab the lock
    // in-between now and |FutexWake()| waking up a waiter. Aside from
    // potentially fairness, this is not an issue, as the newly-awaken thread
    // will check that the lock is still free.
    //
    // There is a small pessimization here though: if we have a single waiter,
    // then when it wakes up, the lock will be set to |kLockedContended|, so
    // when this waiter releases the lock, it will needlessly call
    // |FutexWake()|, even though there are no waiters. This is supported by the
    // kernel, and is what bionic (Android's libc) also does.
    FutexWake();
  }
}

#elif V8_OS_DARWIN

V8_INLINE bool SpinningMutex::TryLock() {
  return os_unfair_lock_trylock(&lock_);
}

V8_INLINE void SpinningMutex::Unlock() { return os_unfair_lock_unlock(&lock_); }

#elif V8_OS_POSIX

V8_INLINE bool SpinningMutex::TryLock() {
  int retval = pthread_mutex_trylock(&lock_);
  DCHECK(retval == 0 || retval == EBUSY);
  return retval == 0;
}

V8_INLINE void SpinningMutex::Unlock() {
  int retval = pthread_mutex_unlock(&lock_);
  USE(retval);
  DCHECK_EQ(0, retval);
}

#elif V8_OS_FUCHSIA

V8_INLINE bool SpinningMutex::TryLock() {
  return sync_mutex_trylock(&lock_) == ZX_OK;
}

V8_INLINE void SpinningMutex::Unlock() { sync_mutex_unlock(&lock_); }

#elif !V8_OS_WIN

V8_INLINE bool SpinningMutex::TryLock() {
  // Possibly faster than CAS. The theory is that if the cacheline is shared,
  // then it can stay shared, for the contended case.
  return !lock_.load(std::memory_order_relaxed) &&
         !lock_.exchange(true, std::memory_order_acquire);
}

V8_INLINE void SpinningMutex::Unlock() {
  lock_.store(false, std::memory_order_release);
}

#endif


// POD Mutex initialized lazily (i.e. the first time Pointer() is called).
// Usage:
//   static LazyMutex my_mutex = LAZY_MUTEX_INITIALIZER;
//
//   void my_function() {
//     MutexGuard guard(my_mutex.Pointer());
//     // Do something.
//   }
//
using LazyMutex = LazyStaticInstance<Mutex, DefaultConstructTrait<Mutex>,
                                     ThreadSafeInitOnceTrait>::type;
#define LAZY_MUTEX_INITIALIZER LAZY_STATIC_INSTANCE_INITIALIZER
using LazySpinningMutex =
    LazyStaticInstance<SpinningMutex, DefaultConstructTrait<SpinningMutex>,
                       ThreadSafeInitOnceTrait>::type;

#define LAZY_SELFISH_MUTEX_INITIALIZER LAZY_STATIC_INSTANCE_INITIALIZER

// RecursiveMutex - a replacement for std::recursive_mutex
//
// This class is a synchronization primitive that can be used to protect shared
// data from being simultaneously accessed by multiple threads. A recursive
// mutex offers exclusive, recursive ownership semantics:
// - A calling thread owns a recursive mutex for a period of time that starts
//   when it successfully calls either |Lock()| or |TryLock()|. During this
//   period, the thread may make additional calls to |Lock()| or |TryLock()|.
//   The period of ownership ends when the thread makes a matching number of
//   calls to |Unlock()|.
// - When a thread owns a recursive mutex, all other threads will block (for
//   calls to |Lock()|) or receive a |false| return value (for |TryLock()|) if
//   they attempt to claim ownership of the recursive mutex.
// - The maximum number of times that a recursive mutex may be locked is
//   unspecified, but after that number is reached, calls to |Lock()| will
//   probably abort the process and calls to |TryLock()| return false.
// The behavior of a program is undefined if a recursive mutex is destroyed
// while still owned by some thread. The RecursiveMutex class is non-copyable.

class V8_BASE_EXPORT RecursiveMutex final {
 public:
  RecursiveMutex();
  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;
  ~RecursiveMutex();

  // Locks the mutex. If another thread has already locked the mutex, a call to
  // |Lock()| will block execution until the lock is acquired. A thread may call
  // |Lock()| on a recursive mutex repeatedly. Ownership will only be released
  // after the thread makes a matching number of calls to |Unlock()|.
  // The behavior is undefined if the mutex is not unlocked before being
  // destroyed, i.e. some thread still owns it.
  void Lock();

  // Unlocks the mutex if its level of ownership is 1 (there was exactly one
  // more call to |Lock()| than there were calls to unlock() made by this
  // thread), reduces the level of ownership by 1 otherwise. The mutex must be
  // locked by the current thread of execution, otherwise, the behavior is
  // undefined.
  void Unlock();

  // Tries to lock the given mutex. Returns whether the mutex was
  // successfully locked.
  // Note: Instead of `DCHECK(!mutex.TryLock())` use `mutex.AssertHeld()`.
  bool TryLock() V8_WARN_UNUSED_RESULT;

  V8_INLINE void AssertHeld() const {
    // If this access results in a race condition being detected by TSan, this
    // mean that you in fact did *not* hold the mutex.
    DCHECK_LT(0, level_);
  }

 private:
  // The implementation-defined native handle type.
#if V8_OS_POSIX
  using NativeHandle = pthread_mutex_t;
#elif V8_OS_WIN
  using NativeHandle = V8_CRITICAL_SECTION;
#elif V8_OS_STARBOARD
  using NativeHandle = starboard::RecursiveMutex;
#endif

  NativeHandle native_handle_;
#ifdef DEBUG
  // This is being used for Assert* methods. Accesses are only allowed if you
  // actually hold the mutex, otherwise you would get race conditions.
  int level_;
#endif
};


// POD RecursiveMutex initialized lazily (i.e. the first time Pointer() is
// called).
// Usage:
//   static LazyRecursiveMutex my_mutex = LAZY_RECURSIVE_MUTEX_INITIALIZER;
//
//   void my_function() {
//     LockGuard<RecursiveMutex> guard(my_mutex.Pointer());
//     // Do something.
//   }
//
using LazyRecursiveMutex =
    LazyStaticInstance<RecursiveMutex, DefaultConstructTrait<RecursiveMutex>,
                       ThreadSafeInitOnceTrait>::type;

#define LAZY_RECURSIVE_MUTEX_INITIALIZER LAZY_STATIC_INSTANCE_INITIALIZER

// SharedMutex - a replacement for std::shared_mutex
//
// This class is a synchronization primitive that can be used to protect shared
// data from being simultaneously accessed by multiple threads. In contrast to
// other mutex types which facilitate exclusive access, a shared_mutex has two
// levels of access:
// - shared: several threads can share ownership of the same mutex.
// - exclusive: only one thread can own the mutex.
// Shared mutexes are usually used in situations when multiple readers can
// access the same resource at the same time without causing data races, but
// only one writer can do so.
// The SharedMutex class is non-copyable.

class V8_BASE_EXPORT SharedMutex final {
 public:
  SharedMutex();
  SharedMutex(const SharedMutex&) = delete;
  SharedMutex& operator=(const SharedMutex&) = delete;
  ~SharedMutex();

  // Acquires shared ownership of the {SharedMutex}. If another thread is
  // holding the mutex in exclusive ownership, a call to {LockShared()} will
  // block execution until shared ownership can be acquired.
  // If {LockShared()} is called by a thread that already owns the mutex in any
  // mode (exclusive or shared), the behavior is undefined and outright fails
  // with dchecks on.
  void LockShared();

  // Locks the SharedMutex. If another thread has already locked the mutex, a
  // call to {LockExclusive()} will block execution until the lock is acquired.
  // If {LockExclusive()} is called by a thread that already owns the mutex in
  // any mode (shared or exclusive), the behavior is undefined and outright
  // fails with dchecks on.
  void LockExclusive();

  // Releases the {SharedMutex} from shared ownership by the calling thread.
  // The mutex must be locked by the current thread of execution in shared mode,
  // otherwise, the behavior is undefined and outright fails with dchecks on.
  void UnlockShared();

  // Unlocks the {SharedMutex}. It must be locked by the current thread of
  // execution, otherwise, the behavior is undefined and outright fails with
  // dchecks on.
  void UnlockExclusive();

  // Tries to lock the {SharedMutex} in shared mode. Returns immediately. On
  // successful lock acquisition returns true, otherwise returns false.
  // This function is allowed to fail spuriously and return false even if the
  // mutex is not currenly exclusively locked by any other thread.
  // If it is called by a thread that already owns the mutex in any mode
  // (shared or exclusive), the behavior is undefined, and outright fails with
  // dchecks on.
  bool TryLockShared() V8_WARN_UNUSED_RESULT;

  // Tries to lock the {SharedMutex}. Returns immediately. On successful lock
  // acquisition returns true, otherwise returns false.
  // This function is allowed to fail spuriously and return false even if the
  // mutex is not currently locked by any other thread.
  // If it is called by a thread that already owns the mutex in any mode
  // (shared or exclusive), the behavior is undefined, and outright fails with
  // dchecks on.
  bool TryLockExclusive() V8_WARN_UNUSED_RESULT;

 private:
  // The implementation-defined native handle type.
#if V8_OS_DARWIN
  // pthread_rwlock_t is broken on MacOS when signals are being sent to the
  // process (see https://crbug.com/v8/11399).
  // We thus use std::shared_mutex on MacOS, which does not have this problem.
  using NativeHandle = absl::Mutex;
#elif V8_OS_POSIX
  using NativeHandle = pthread_rwlock_t;
#elif V8_OS_WIN
  using NativeHandle = V8_SRWLOCK;
#elif V8_OS_STARBOARD
  using NativeHandle = starboard::RWLock;
#endif

  NativeHandle native_handle_;
};

// LockGuard
//
// This class is a mutex wrapper that provides a convenient RAII-style mechanism
// for owning a mutex for the duration of a scoped block.
// When a LockGuard object is created, it attempts to take ownership of the
// mutex it is given. When control leaves the scope in which the LockGuard
// object was created, the LockGuard is destructed and the mutex is released.
// The LockGuard class is non-copyable.

// Controls whether a LockGuard always requires a valid Mutex or will just
// ignore it if it's nullptr.
enum class NullBehavior { kRequireNotNull, kIgnoreIfNull };

template <typename Mutex, NullBehavior Behavior = NullBehavior::kRequireNotNull>
class V8_NODISCARD LockGuard final {
 public:
  explicit LockGuard(Mutex* mutex) : mutex_(mutex) {
    DCHECK_IMPLIES(Behavior == NullBehavior::kRequireNotNull,
                   mutex_ != nullptr);
    if (has_mutex()) mutex_->Lock();
  }
  explicit LockGuard(Mutex& mutex) : mutex_(&mutex) {
    // `mutex_` is guaranteed to be non-null here.
    mutex_->Lock();
  }
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;
  LockGuard(LockGuard&& other) V8_NOEXCEPT : mutex_(other.mutex_) {
    DCHECK_IMPLIES(Behavior == NullBehavior::kRequireNotNull,
                   mutex_ != nullptr);
    other.mutex_ = nullptr;
  }
  ~LockGuard() {
    if (has_mutex()) mutex_->Unlock();
  }

 private:
  Mutex* mutex_;

  bool V8_INLINE has_mutex() const { return mutex_ != nullptr; }
};

using MutexGuard = LockGuard<Mutex>;
using SpinningMutexGuard = LockGuard<SpinningMutex>;
using RecursiveMutexGuard = LockGuard<RecursiveMutex>;

enum MutexSharedType : bool { kShared = true, kExclusive = false };

template <MutexSharedType kIsShared,
          NullBehavior Behavior = NullBehavior::kRequireNotNull>
class V8_NODISCARD SharedMutexGuard final {
 public:
  explicit SharedMutexGuard(SharedMutex* mutex) : mutex_(mutex) {
    if (!has_mutex()) return;
    if (kIsShared) {
      mutex_->LockShared();
    } else {
      mutex_->LockExclusive();
    }
  }
  SharedMutexGuard(const SharedMutexGuard&) = delete;
  SharedMutexGuard& operator=(const SharedMutexGuard&) = delete;
  ~SharedMutexGuard() {
    if (!has_mutex()) return;
    if (kIsShared) {
      mutex_->UnlockShared();
    } else {
      mutex_->UnlockExclusive();
    }
  }

 private:
  SharedMutex* const mutex_;

  bool V8_INLINE has_mutex() const {
    DCHECK_IMPLIES(Behavior == NullBehavior::kRequireNotNull,
                   mutex_ != nullptr);
    return Behavior == NullBehavior::kRequireNotNull || mutex_ != nullptr;
  }
};

template <MutexSharedType kIsShared,
          NullBehavior Behavior = NullBehavior::kRequireNotNull>
class V8_NODISCARD SharedMutexGuardIf final {
 public:
  SharedMutexGuardIf(SharedMutex* mutex, bool enable_mutex) {
    if (enable_mutex) mutex_.emplace(mutex);
  }
  SharedMutexGuardIf(const SharedMutexGuardIf&) = delete;
  SharedMutexGuardIf& operator=(const SharedMutexGuardIf&) = delete;

 private:
  std::optional<SharedMutexGuard<kIsShared, Behavior>> mutex_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_MUTEX_H_
