// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/mutex.h"

#include <errno.h>

#include <atomic>

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/time.h"
#include "src/base/platform/yield-processor.h"

#if DEBUG
#include <unordered_set>
#endif  // DEBUG

#if V8_OS_WIN
#include <windows.h>
#endif

#if V8_TARGET_OS_LINUX
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#if !V8_TARGET_OS_LINUX && !V8_OS_WIN && !V8_OS_DARWIN && !V8_OS_POSIX && \
    !V8_OS_FUCHSIA

#if V8_OS_STARBOARD
#include "starboard/thread.h"
#define V8_YIELD_THREAD SbThreadYield()
#else  // Other OS
#warning "Thread yield not supported on this OS."
#define V8_YIELD_THREAD ((void)0)
#endif

#endif

// The YIELD_PROCESSOR macro wraps an architecture specific-instruction that
// informs the processor we're in a busy wait, so it can handle the branch more
// intelligently and e.g. reduce power to our core or give more resources to the
// other hyper-thread on this core. See the following for context:
// https://software.intel.com/en-us/articles/benefitting-power-and-performance-sleep-loops

namespace v8 {
namespace base {

void SpinningMutex::AcquireSpinThenBlock() {
  int tries = 0;
  int backoff = 1;
  do {
    if (TryLock()) [[likely]] {
      return;
    }
    // Note: Per the intel optimization manual
    // (https://software.intel.com/content/dam/develop/public/us/en/documents/64-ia-32-architectures-optimization-manual.pdf),
    // the "pause" instruction is more costly on Skylake Client than on previous
    // architectures. The latency is found to be 141 cycles
    // there (from ~10 on previous ones, nice 14x).
    //
    // According to Agner Fog's instruction tables, the latency is still >100
    // cycles on Ice Lake, and from other sources, seems to be high as well on
    // Alder Lake. Separately, it is (from
    // https://agner.org/optimize/instruction_tables.pdf) also high on AMD Zen 3
    // (~65). So just assume that it's this way for most x86_64 architectures.
    //
    // Also, loop several times here, following the guidelines in section 2.3.4
    // of the manual, "Pause latency in Skylake Client Microarchitecture".
    for (int yields = 0; yields < backoff; yields++) {
      YIELD_PROCESSOR;
      tries++;
    }
    constexpr int kMaxBackoff = 16;
    backoff = std::min(kMaxBackoff, backoff << 1);
  } while (tries < kSpinCount);

  LockSlow();
}

#if V8_TARGET_OS_LINUX

void SpinningMutex::FutexWait() {
  // Save and restore errno.
  int saved_errno = errno;
  // Don't check the return value, as we will not be awaken by a timeout, since
  // none is specified.
  //
  // Ignoring the return value doesn't impact correctness, as this acts as an
  // immediate wakeup. For completeness, the possible errors for FUTEX_WAIT are:
  // - EACCES: state_ is not readable. Should not happen.
  // - EAGAIN: the value is not as expected, that is not |kLockedContended|, in
  //           which case retrying the loop is the right behavior.
  // - EINTR: signal, looping is the right behavior.
  // - EINVAL: invalid argument.
  //
  // Note: not checking the return value is the approach used in bionic and
  // glibc as well.
  //
  // Will return immediately if |state_| is no longer equal to
  // |kLockedContended|. Otherwise, sleeps and wakes up when |state_| may not be
  // |kLockedContended| anymore. Note that even without spurious wakeups, the
  // value of |state_| is not guaranteed when this returns, as another thread
  // may get the lock before we get to run.
  if (syscall(SYS_futex, &state_, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
              kLockedContended, nullptr, nullptr, 0) != 0) {
    // These are programming error, check them.
    DCHECK_NE(EACCES, errno);
    DCHECK_NE(EINVAL, errno);
  }
  errno = saved_errno;
}

void SpinningMutex::FutexWake() {
  int saved_errno = errno;
  CHECK_NE(-1, syscall(SYS_futex, &state_, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                       1 /* wake up a single waiter */, nullptr, nullptr, 0));
  errno = saved_errno;
}

void SpinningMutex::LockSlow() {
  // If this thread gets awaken but another one got the lock first, then go back
  // to sleeping. See comments in |FutexWait()| to see why a loop is required.
  while (state_.exchange(kLockedContended, std::memory_order_acquire) !=
         kUnlocked) {
    FutexWait();
  }
}

#elif V8_OS_WIN

#elif V8_OS_DARWIN

// TODO(verwaest): We should use the constants from the header, but they aren't
// exposed until macOS 15.
#define OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION 0x00010000
#define OS_UNFAIR_LOCK_ADAPTIVE_SPIN 0x00040000

typedef uint32_t os_unfair_lock_options_t;

extern "C" {
void __attribute__((weak))
os_unfair_lock_lock_with_options(os_unfair_lock* lock,
                                 os_unfair_lock_options_t);
}

void SpinningMutex::LockSlow() {
  if (os_unfair_lock_lock_with_options) {
    const os_unfair_lock_options_t options =
        static_cast<os_unfair_lock_options_t>(
            OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
    os_unfair_lock_lock_with_options(&lock_, options);
  } else {
    os_unfair_lock_lock(&lock_);
  }
}

#elif V8_OS_POSIX

void SpinningMutex::LockSlow() {
  int retval = pthread_mutex_lock(&lock_);
  USE(retval);
  DCHECK_EQ(0, retval);
}

#elif V8_OS_FUCHSIA

void SpinningMutex::LockSlow() { sync_mutex_lock(&lock_); }

#else

void SpinningMutex::LockSlow() {
  int yield_thread_count = 0;
  do {
    if (yield_thread_count < 10) {
      V8_YIELD_THREAD;
      yield_thread_count++;
    } else {
      // At this point, it's likely that the lock is held by a lower priority
      // thread that is unavailable to finish its work because of higher
      // priority threads spinning here. Sleeping should ensure that they make
      // progress.
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  } while (!TryLock());
}

#endif

#if DEBUG
namespace {
// Used for asserts to guarantee we are not re-locking a mutex on the same
// thread. If this thread has only one held shared mutex (common case), we use
// {single_held_shared_mutex}. If it has more than one we allocate a set for it.
// Said set has to manually be constructed and destroyed.
thread_local base::SharedMutex* single_held_shared_mutex = nullptr;
using TSet = std::unordered_set<base::SharedMutex*>;
thread_local TSet* held_shared_mutexes = nullptr;

// Returns true iff {shared_mutex} is not a held mutex.
bool SharedMutexNotHeld(SharedMutex* shared_mutex) {
  DCHECK_NOT_NULL(shared_mutex);
  return single_held_shared_mutex != shared_mutex &&
         (!held_shared_mutexes ||
          held_shared_mutexes->count(shared_mutex) == 0);
}

// Tries to hold {shared_mutex}. Returns true iff it hadn't been held prior to
// this function call.
bool TryHoldSharedMutex(SharedMutex* shared_mutex) {
  DCHECK_NOT_NULL(shared_mutex);
  if (single_held_shared_mutex) {
    if (shared_mutex == single_held_shared_mutex) {
      return false;
    }
    DCHECK_NULL(held_shared_mutexes);
    held_shared_mutexes = new TSet({single_held_shared_mutex, shared_mutex});
    single_held_shared_mutex = nullptr;
    return true;
  } else if (held_shared_mutexes) {
    return held_shared_mutexes->insert(shared_mutex).second;
  } else {
    DCHECK_NULL(single_held_shared_mutex);
    single_held_shared_mutex = shared_mutex;
    return true;
  }
}

// Tries to release {shared_mutex}. Returns true iff it had been held prior to
// this function call.
bool TryReleaseSharedMutex(SharedMutex* shared_mutex) {
  DCHECK_NOT_NULL(shared_mutex);
  if (single_held_shared_mutex == shared_mutex) {
    single_held_shared_mutex = nullptr;
    return true;
  }
  if (held_shared_mutexes && held_shared_mutexes->erase(shared_mutex)) {
    if (held_shared_mutexes->empty()) {
      delete held_shared_mutexes;
      held_shared_mutexes = nullptr;
    }
    return true;
  }
  return false;
}
}  // namespace
#endif  // DEBUG

#if V8_OS_POSIX

static V8_INLINE void InitializeNativeHandle(pthread_mutex_t* mutex) {
  int result;
#if defined(DEBUG)
  // Use an error checking mutex in debug mode.
  pthread_mutexattr_t attr;
  result = pthread_mutexattr_init(&attr);
  DCHECK_EQ(0, result);
  result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  DCHECK_EQ(0, result);
  result = pthread_mutex_init(mutex, &attr);
  DCHECK_EQ(0, result);
  result = pthread_mutexattr_destroy(&attr);
#else
  // Use a fast mutex (default attributes).
  result = pthread_mutex_init(mutex, nullptr);
#endif  // defined(DEBUG)
  DCHECK_EQ(0, result);
  USE(result);
}


static V8_INLINE void InitializeRecursiveNativeHandle(pthread_mutex_t* mutex) {
  pthread_mutexattr_t attr;
  int result = pthread_mutexattr_init(&attr);
  DCHECK_EQ(0, result);
  result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  DCHECK_EQ(0, result);
  result = pthread_mutex_init(mutex, &attr);
  DCHECK_EQ(0, result);
  result = pthread_mutexattr_destroy(&attr);
  DCHECK_EQ(0, result);
  USE(result);
}


static V8_INLINE void DestroyNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_destroy(mutex);
  DCHECK_EQ(0, result);
  USE(result);
}


static V8_INLINE void LockNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_lock(mutex);
  DCHECK_EQ(0, result);
  USE(result);
}


static V8_INLINE void UnlockNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_unlock(mutex);
  DCHECK_EQ(0, result);
  USE(result);
}


static V8_INLINE bool TryLockNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_trylock(mutex);
  if (result == EBUSY) {
    return false;
  }
  DCHECK_EQ(0, result);
  return true;
}

#ifdef V8_OS_DARWIN
SpinningMutex::SpinningMutex() : lock_(OS_UNFAIR_LOCK_INIT) {}
#elif V8_TARGET_OS_LINUX
SpinningMutex::SpinningMutex() = default;
#else
SpinningMutex::SpinningMutex() : lock_(PTHREAD_MUTEX_INITIALIZER) {}
#endif

Mutex::Mutex() {
  InitializeNativeHandle(&native_handle_);
#ifdef DEBUG
  level_ = 0;
#endif
}


Mutex::~Mutex() {
  DestroyNativeHandle(&native_handle_);
  DCHECK_EQ(0, level_);
}


void Mutex::Lock() {
  LockNativeHandle(&native_handle_);
  AssertUnheldAndMark();
}


void Mutex::Unlock() {
  AssertHeldAndUnmark();
  UnlockNativeHandle(&native_handle_);
}


bool Mutex::TryLock() {
  if (!TryLockNativeHandle(&native_handle_)) {
    return false;
  }
  AssertUnheldAndMark();
  return true;
}

RecursiveMutex::RecursiveMutex() {
  InitializeRecursiveNativeHandle(&native_handle_);
#ifdef DEBUG
  level_ = 0;
#endif
}


RecursiveMutex::~RecursiveMutex() {
  DestroyNativeHandle(&native_handle_);
  DCHECK_EQ(0, level_);
}


void RecursiveMutex::Lock() {
  LockNativeHandle(&native_handle_);
#ifdef DEBUG
  DCHECK_LE(0, level_);
  level_++;
#endif
}


void RecursiveMutex::Unlock() {
#ifdef DEBUG
  DCHECK_LT(0, level_);
  level_--;
#endif
  UnlockNativeHandle(&native_handle_);
}


bool RecursiveMutex::TryLock() {
  if (!TryLockNativeHandle(&native_handle_)) {
    return false;
  }
#ifdef DEBUG
  DCHECK_LE(0, level_);
  level_++;
#endif
  return true;
}

#if V8_OS_DARWIN

SharedMutex::SharedMutex() = default;
SharedMutex::~SharedMutex() = default;

void SharedMutex::LockShared() ABSL_SHARED_LOCK_FUNCTION(native_handle_) {
  DCHECK(TryHoldSharedMutex(this));
  native_handle_.ReaderLock();
}

void SharedMutex::LockExclusive() ABSL_EXCLUSIVE_LOCK_FUNCTION(native_handle_) {
  DCHECK(TryHoldSharedMutex(this));
  native_handle_.Lock();
}

void SharedMutex::UnlockShared() ABSL_UNLOCK_FUNCTION(native_handle_) {
  DCHECK(TryReleaseSharedMutex(this));
  native_handle_.ReaderUnlock();
}

void SharedMutex::UnlockExclusive() ABSL_UNLOCK_FUNCTION(native_handle_) {
  DCHECK(TryReleaseSharedMutex(this));
  native_handle_.Unlock();
}

bool SharedMutex::TryLockShared()
    ABSL_SHARED_TRYLOCK_FUNCTION(true, native_handle_) {
  DCHECK(SharedMutexNotHeld(this));
  bool result = native_handle_.ReaderTryLock();
  if (result) DCHECK(TryHoldSharedMutex(this));
  return result;
}

bool SharedMutex::TryLockExclusive()
    ABSL_EXCLUSIVE_TRYLOCK_FUNCTION(true, native_handle_) {
  DCHECK(SharedMutexNotHeld(this));
  bool result = native_handle_.TryLock();
  if (result) DCHECK(TryHoldSharedMutex(this));
  return result;
}

#else  // !V8_OS_DARWIN

SharedMutex::SharedMutex() { pthread_rwlock_init(&native_handle_, nullptr); }

SharedMutex::~SharedMutex() {
  int result = pthread_rwlock_destroy(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::LockShared() {
  DCHECK(TryHoldSharedMutex(this));
  int result = pthread_rwlock_rdlock(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::LockExclusive() {
  DCHECK(TryHoldSharedMutex(this));
  int result = pthread_rwlock_wrlock(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::UnlockShared() {
  DCHECK(TryReleaseSharedMutex(this));
  int result = pthread_rwlock_unlock(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::UnlockExclusive() {
  // Same code as {UnlockShared} on POSIX.
  UnlockShared();
}

bool SharedMutex::TryLockShared() {
  DCHECK(SharedMutexNotHeld(this));
  bool result = pthread_rwlock_tryrdlock(&native_handle_) == 0;
  if (result) DCHECK(TryHoldSharedMutex(this));
  return result;
}

bool SharedMutex::TryLockExclusive() {
  DCHECK(SharedMutexNotHeld(this));
  bool result = pthread_rwlock_trywrlock(&native_handle_) == 0;
  if (result) DCHECK(TryHoldSharedMutex(this));
  return result;
}

#endif  // !V8_OS_DARWIN

#elif V8_OS_WIN

void SpinningMutex::LockSlow() {
  AcquireSRWLockExclusive(V8ToWindowsType(&lock_));
}

bool SpinningMutex::TryLock() {
  return !!TryAcquireSRWLockExclusive(V8ToWindowsType(&lock_));
}

void SpinningMutex::Unlock() {
  ReleaseSRWLockExclusive(V8ToWindowsType(&lock_));
}

SpinningMutex::SpinningMutex() : lock_(SRWLOCK_INIT) {}

Mutex::Mutex() : native_handle_(SRWLOCK_INIT) {
#ifdef DEBUG
  level_ = 0;
#endif
}


Mutex::~Mutex() {
  DCHECK_EQ(0, level_);
}


void Mutex::Lock() {
  AcquireSRWLockExclusive(V8ToWindowsType(&native_handle_));
  AssertUnheldAndMark();
}


void Mutex::Unlock() {
  AssertHeldAndUnmark();
  ReleaseSRWLockExclusive(V8ToWindowsType(&native_handle_));
}


bool Mutex::TryLock() {
  if (!TryAcquireSRWLockExclusive(V8ToWindowsType(&native_handle_))) {
    return false;
  }
  AssertUnheldAndMark();
  return true;
}

RecursiveMutex::RecursiveMutex() {
  InitializeCriticalSection(V8ToWindowsType(&native_handle_));
#ifdef DEBUG
  level_ = 0;
#endif
}


RecursiveMutex::~RecursiveMutex() {
  DeleteCriticalSection(V8ToWindowsType(&native_handle_));
  DCHECK_EQ(0, level_);
}


void RecursiveMutex::Lock() {
  EnterCriticalSection(V8ToWindowsType(&native_handle_));
#ifdef DEBUG
  DCHECK_LE(0, level_);
  level_++;
#endif
}


void RecursiveMutex::Unlock() {
#ifdef DEBUG
  DCHECK_LT(0, level_);
  level_--;
#endif
  LeaveCriticalSection(V8ToWindowsType(&native_handle_));
}


bool RecursiveMutex::TryLock() {
  if (!TryEnterCriticalSection(V8ToWindowsType(&native_handle_))) {
    return false;
  }
#ifdef DEBUG
  DCHECK_LE(0, level_);
  level_++;
#endif
  return true;
}

SharedMutex::SharedMutex() : native_handle_(SRWLOCK_INIT) {}

SharedMutex::~SharedMutex() {}

void SharedMutex::LockShared() {
  DCHECK(TryHoldSharedMutex(this));
  AcquireSRWLockShared(V8ToWindowsType(&native_handle_));
}

void SharedMutex::LockExclusive() {
  DCHECK(TryHoldSharedMutex(this));
  AcquireSRWLockExclusive(V8ToWindowsType(&native_handle_));
}

void SharedMutex::UnlockShared() {
  DCHECK(TryReleaseSharedMutex(this));
  ReleaseSRWLockShared(V8ToWindowsType(&native_handle_));
}

void SharedMutex::UnlockExclusive() {
  DCHECK(TryReleaseSharedMutex(this));
  ReleaseSRWLockExclusive(V8ToWindowsType(&native_handle_));
}

bool SharedMutex::TryLockShared() {
  DCHECK(SharedMutexNotHeld(this));
  bool result = TryAcquireSRWLockShared(V8ToWindowsType(&native_handle_));
  if (result) DCHECK(TryHoldSharedMutex(this));
  return result;
}

bool SharedMutex::TryLockExclusive() {
  DCHECK(SharedMutexNotHeld(this));
  bool result = TryAcquireSRWLockExclusive(V8ToWindowsType(&native_handle_));
  if (result) DCHECK(TryHoldSharedMutex(this));
  return result;
}

#elif V8_OS_STARBOARD

SpinningMutex::SpinningMutex() = default;

Mutex::Mutex() { SbMutexCreate(&native_handle_); }

Mutex::~Mutex() { SbMutexDestroy(&native_handle_); }

void Mutex::Lock() { SbMutexAcquire(&native_handle_); }

void Mutex::Unlock() { SbMutexRelease(&native_handle_); }

RecursiveMutex::RecursiveMutex() {}

RecursiveMutex::~RecursiveMutex() {}

void RecursiveMutex::Lock() { native_handle_.Acquire(); }

void RecursiveMutex::Unlock() { native_handle_.Release(); }

bool RecursiveMutex::TryLock() { return native_handle_.AcquireTry(); }

SharedMutex::SharedMutex() = default;

SharedMutex::~SharedMutex() = default;

void SharedMutex::LockShared() {
  DCHECK(TryHoldSharedMutex(this));
  native_handle_.AcquireReadLock();
}

void SharedMutex::LockExclusive() {
  DCHECK(TryHoldSharedMutex(this));
  native_handle_.AcquireWriteLock();
}

void SharedMutex::UnlockShared() {
  DCHECK(TryReleaseSharedMutex(this));
  native_handle_.ReleaseReadLock();
}

void SharedMutex::UnlockExclusive() {
  DCHECK(TryReleaseSharedMutex(this));
  native_handle_.ReleaseWriteLock();
}

bool SharedMutex::TryLockShared() {
  DCHECK(SharedMutexNotHeld(this));
  return false;
}

bool SharedMutex::TryLockExclusive() {
  DCHECK(SharedMutexNotHeld(this));
  return false;
}
#endif  // V8_OS_STARBOARD

}  // namespace base
}  // namespace v8
