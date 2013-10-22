// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "platform/mutex.h"

#include <cerrno>

namespace v8 {
namespace internal {

#if V8_OS_POSIX

static V8_INLINE void InitializeNativeHandle(pthread_mutex_t* mutex) {
  int result;
#if defined(DEBUG)
  // Use an error checking mutex in debug mode.
  pthread_mutexattr_t attr;
  result = pthread_mutexattr_init(&attr);
  ASSERT_EQ(0, result);
  result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  ASSERT_EQ(0, result);
  result = pthread_mutex_init(mutex, &attr);
  ASSERT_EQ(0, result);
  result = pthread_mutexattr_destroy(&attr);
#else
  // Use a fast mutex (default attributes).
  result = pthread_mutex_init(mutex, NULL);
#endif  // defined(DEBUG)
  ASSERT_EQ(0, result);
  USE(result);
}


static V8_INLINE void InitializeRecursiveNativeHandle(pthread_mutex_t* mutex) {
  pthread_mutexattr_t attr;
  int result = pthread_mutexattr_init(&attr);
  ASSERT_EQ(0, result);
  result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  ASSERT_EQ(0, result);
  result = pthread_mutex_init(mutex, &attr);
  ASSERT_EQ(0, result);
  result = pthread_mutexattr_destroy(&attr);
  ASSERT_EQ(0, result);
  USE(result);
}


static V8_INLINE void DestroyNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_destroy(mutex);
  ASSERT_EQ(0, result);
  USE(result);
}


static V8_INLINE void LockNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_lock(mutex);
  ASSERT_EQ(0, result);
  USE(result);
}


static V8_INLINE void UnlockNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_unlock(mutex);
  ASSERT_EQ(0, result);
  USE(result);
}


static V8_INLINE bool TryLockNativeHandle(pthread_mutex_t* mutex) {
  int result = pthread_mutex_trylock(mutex);
  if (result == EBUSY) {
    return false;
  }
  ASSERT_EQ(0, result);
  return true;
}

#elif V8_OS_WIN

static V8_INLINE void InitializeNativeHandle(PCRITICAL_SECTION cs) {
  InitializeCriticalSection(cs);
}


static V8_INLINE void InitializeRecursiveNativeHandle(PCRITICAL_SECTION cs) {
  InitializeCriticalSection(cs);
}


static V8_INLINE void DestroyNativeHandle(PCRITICAL_SECTION cs) {
  DeleteCriticalSection(cs);
}


static V8_INLINE void LockNativeHandle(PCRITICAL_SECTION cs) {
  EnterCriticalSection(cs);
}


static V8_INLINE void UnlockNativeHandle(PCRITICAL_SECTION cs) {
  LeaveCriticalSection(cs);
}


static V8_INLINE bool TryLockNativeHandle(PCRITICAL_SECTION cs) {
  return TryEnterCriticalSection(cs);
}

#endif  // V8_OS_POSIX


Mutex::Mutex() {
  InitializeNativeHandle(&native_handle_);
#ifdef DEBUG
  level_ = 0;
#endif
}


Mutex::~Mutex() {
  DestroyNativeHandle(&native_handle_);
  ASSERT_EQ(0, level_);
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
  ASSERT_EQ(0, level_);
}


void RecursiveMutex::Lock() {
  LockNativeHandle(&native_handle_);
#ifdef DEBUG
  ASSERT_LE(0, level_);
  level_++;
#endif
}


void RecursiveMutex::Unlock() {
#ifdef DEBUG
  ASSERT_LT(0, level_);
  level_--;
#endif
  UnlockNativeHandle(&native_handle_);
}


bool RecursiveMutex::TryLock() {
  if (!TryLockNativeHandle(&native_handle_)) {
    return false;
  }
#ifdef DEBUG
  ASSERT_LE(0, level_);
  level_++;
#endif
  return true;
}

} }  // namespace v8::internal
