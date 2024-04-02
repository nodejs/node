// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/mutex.h"

#include <errno.h>

#if DEBUG
#include <unordered_set>
#endif  // DEBUG

#if V8_OS_WIN
#include <windows.h>
#endif

namespace v8 {
namespace base {

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

SharedMutex::SharedMutex() { InitializeNativeHandle(&native_handle_); }

SharedMutex::~SharedMutex() { DestroyNativeHandle(&native_handle_); }

void SharedMutex::LockShared() { LockExclusive(); }

void SharedMutex::LockExclusive() {
  DCHECK(TryHoldSharedMutex(this));
  LockNativeHandle(&native_handle_);
}

void SharedMutex::UnlockShared() { UnlockExclusive(); }

void SharedMutex::UnlockExclusive() {
  DCHECK(TryReleaseSharedMutex(this));
  UnlockNativeHandle(&native_handle_);
}

bool SharedMutex::TryLockShared() { return TryLockExclusive(); }

bool SharedMutex::TryLockExclusive() {
  DCHECK(SharedMutexNotHeld(this));
  if (!TryLockNativeHandle(&native_handle_)) return false;
  DCHECK(TryHoldSharedMutex(this));
  return true;
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
