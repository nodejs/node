// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/mutex.h"

#include <errno.h>

namespace v8 {
namespace base {

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

SharedMutex::SharedMutex() { pthread_rwlock_init(&native_handle_, nullptr); }

SharedMutex::~SharedMutex() {
  int result = pthread_rwlock_destroy(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::LockShared() {
  int result = pthread_rwlock_rdlock(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::LockExclusive() {
  int result = pthread_rwlock_wrlock(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::UnlockShared() {
  int result = pthread_rwlock_unlock(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void SharedMutex::UnlockExclusive() {
  // Same code as {UnlockShared} on POSIX.
  UnlockShared();
}

bool SharedMutex::TryLockShared() {
  return pthread_rwlock_tryrdlock(&native_handle_) == 0;
}

bool SharedMutex::TryLockExclusive() {
  return pthread_rwlock_trywrlock(&native_handle_) == 0;
}

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
  AcquireSRWLockExclusive(&native_handle_);
  AssertUnheldAndMark();
}


void Mutex::Unlock() {
  AssertHeldAndUnmark();
  ReleaseSRWLockExclusive(&native_handle_);
}


bool Mutex::TryLock() {
  if (!TryAcquireSRWLockExclusive(&native_handle_)) {
    return false;
  }
  AssertUnheldAndMark();
  return true;
}


RecursiveMutex::RecursiveMutex() {
  InitializeCriticalSection(&native_handle_);
#ifdef DEBUG
  level_ = 0;
#endif
}


RecursiveMutex::~RecursiveMutex() {
  DeleteCriticalSection(&native_handle_);
  DCHECK_EQ(0, level_);
}


void RecursiveMutex::Lock() {
  EnterCriticalSection(&native_handle_);
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
  LeaveCriticalSection(&native_handle_);
}


bool RecursiveMutex::TryLock() {
  if (!TryEnterCriticalSection(&native_handle_)) {
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

void SharedMutex::LockShared() { AcquireSRWLockShared(&native_handle_); }

void SharedMutex::LockExclusive() { AcquireSRWLockExclusive(&native_handle_); }

void SharedMutex::UnlockShared() { ReleaseSRWLockShared(&native_handle_); }

void SharedMutex::UnlockExclusive() {
  ReleaseSRWLockExclusive(&native_handle_);
}

bool SharedMutex::TryLockShared() {
  return TryAcquireSRWLockShared(&native_handle_);
}

bool SharedMutex::TryLockExclusive() {
  return TryAcquireSRWLockExclusive(&native_handle_);
}

#endif  // V8_OS_POSIX

}  // namespace base
}  // namespace v8
