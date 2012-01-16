/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"
#include <assert.h>

#define HAVE_SRWLOCK_API() (pTryAcquireSRWLockShared != NULL)

#ifdef _MSC_VER /* msvc */
# define inline __inline
# define NOINLINE __declspec (noinline)
#else  /* gcc */
# define inline inline
# define NOINLINE __attribute__ ((noinline))
#endif


inline static int uv__rwlock_srwlock_init(uv_rwlock_t* rwlock);
inline static void uv__rwlock_srwlock_destroy(uv_rwlock_t* rwlock);
inline static void uv__rwlock_srwlock_rdlock(uv_rwlock_t* rwlock);
inline static int uv__rwlock_srwlock_tryrdlock(uv_rwlock_t* rwlock);
inline static void uv__rwlock_srwlock_rdunlock(uv_rwlock_t* rwlock);
inline static void uv__rwlock_srwlock_wrlock(uv_rwlock_t* rwlock);
inline static int uv__rwlock_srwlock_trywrlock(uv_rwlock_t* rwlock);
inline static void uv__rwlock_srwlock_wrunlock(uv_rwlock_t* rwlock);

inline static int uv__rwlock_fallback_init(uv_rwlock_t* rwlock);
inline static void uv__rwlock_fallback_destroy(uv_rwlock_t* rwlock);
inline static void uv__rwlock_fallback_rdlock(uv_rwlock_t* rwlock);
inline static int uv__rwlock_fallback_tryrdlock(uv_rwlock_t* rwlock);
inline static void uv__rwlock_fallback_rdunlock(uv_rwlock_t* rwlock);
inline static void uv__rwlock_fallback_wrlock(uv_rwlock_t* rwlock);
inline static int uv__rwlock_fallback_trywrlock(uv_rwlock_t* rwlock);
inline static void uv__rwlock_fallback_wrunlock(uv_rwlock_t* rwlock);


static NOINLINE void uv__once_inner(uv_once_t* guard,
    void (*callback)(void)) {
  DWORD result;
  HANDLE existing_event, created_event;
  HANDLE* event_ptr;

  /* Fetch and align event_ptr */
  event_ptr = (HANDLE*) (((uintptr_t) &guard->event + (sizeof(HANDLE) - 1)) &
    ~(sizeof(HANDLE) - 1));

  created_event = CreateEvent(NULL, 1, 0, NULL);
  if (created_event == 0) {
    /* Could fail in a low-memory situation? */
    uv_fatal_error(GetLastError(), "CreateEvent");
  }

  existing_event = InterlockedCompareExchangePointer(event_ptr,
                                                     created_event,
                                                     NULL);

  if (existing_event == NULL) {
    /* We won the race */
    callback();

    result = SetEvent(created_event);
    assert(result);
    guard->ran = 1;

  } else {
    /* We lost the race. Destroy the event we created and wait for the */
    /* existing one to become signaled. */
    CloseHandle(created_event);
    result = WaitForSingleObject(existing_event, INFINITE);
    assert(result == WAIT_OBJECT_0);
  }
}


void uv_once(uv_once_t* guard, void (*callback)(void)) {
  /* Fast case - avoid WaitForSingleObject. */
  if (guard->ran) {
    return;
  }

  uv__once_inner(guard, callback);
}


int uv_thread_join(uv_thread_t *tid) {
  if (WaitForSingleObject(*tid, INFINITE))
    return -1;
  else {
    CloseHandle(*tid);
    *tid = 0;
    return 0;
  }
}


int uv_mutex_init(uv_mutex_t* mutex) {
  InitializeCriticalSection(mutex);
  return 0;
}


void uv_mutex_destroy(uv_mutex_t* mutex) {
  DeleteCriticalSection(mutex);
}


void uv_mutex_lock(uv_mutex_t* mutex) {
  EnterCriticalSection(mutex);
}


int uv_mutex_trylock(uv_mutex_t* mutex) {
  if (TryEnterCriticalSection(mutex))
    return 0;
  else
    return -1;
}


void uv_mutex_unlock(uv_mutex_t* mutex) {
  LeaveCriticalSection(mutex);
}


int uv_rwlock_init(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    return uv__rwlock_srwlock_init(rwlock);
  else
    return uv__rwlock_fallback_init(rwlock);
}


void uv_rwlock_destroy(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    uv__rwlock_srwlock_destroy(rwlock);
  else
    uv__rwlock_fallback_destroy(rwlock);
}


void uv_rwlock_rdlock(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    uv__rwlock_srwlock_rdlock(rwlock);
  else
    uv__rwlock_fallback_rdlock(rwlock);
}


int uv_rwlock_tryrdlock(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    return uv__rwlock_srwlock_tryrdlock(rwlock);
  else
    return uv__rwlock_fallback_tryrdlock(rwlock);
}


void uv_rwlock_rdunlock(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    uv__rwlock_srwlock_rdunlock(rwlock);
  else
    uv__rwlock_fallback_rdunlock(rwlock);
}


void uv_rwlock_wrlock(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    uv__rwlock_srwlock_wrlock(rwlock);
  else
    uv__rwlock_fallback_wrlock(rwlock);
}


int uv_rwlock_trywrlock(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    return uv__rwlock_srwlock_trywrlock(rwlock);
  else
    return uv__rwlock_fallback_trywrlock(rwlock);
}


void uv_rwlock_wrunlock(uv_rwlock_t* rwlock) {
  if (HAVE_SRWLOCK_API())
    uv__rwlock_srwlock_wrunlock(rwlock);
  else
    uv__rwlock_fallback_wrunlock(rwlock);
}


inline static int uv__rwlock_srwlock_init(uv_rwlock_t* rwlock) {
  pInitializeSRWLock(&rwlock->srwlock_);
  return 0;
}


inline static void uv__rwlock_srwlock_destroy(uv_rwlock_t* rwlock) {
  (void) rwlock;
}


inline static void uv__rwlock_srwlock_rdlock(uv_rwlock_t* rwlock) {
  pAcquireSRWLockShared(&rwlock->srwlock_);
}


inline static int uv__rwlock_srwlock_tryrdlock(uv_rwlock_t* rwlock) {
  if (pTryAcquireSRWLockShared(&rwlock->srwlock_))
    return 0;
  else
    return -1;
}


inline static void uv__rwlock_srwlock_rdunlock(uv_rwlock_t* rwlock) {
  pReleaseSRWLockShared(&rwlock->srwlock_);
}


inline static void uv__rwlock_srwlock_wrlock(uv_rwlock_t* rwlock) {
  pAcquireSRWLockExclusive(&rwlock->srwlock_);
}


inline static int uv__rwlock_srwlock_trywrlock(uv_rwlock_t* rwlock) {
  if (pTryAcquireSRWLockExclusive(&rwlock->srwlock_))
    return 0;
  else
    return -1;
}


inline static void uv__rwlock_srwlock_wrunlock(uv_rwlock_t* rwlock) {
  pReleaseSRWLockExclusive(&rwlock->srwlock_);
}


inline static int uv__rwlock_fallback_init(uv_rwlock_t* rwlock) {
  if (uv_mutex_init(&rwlock->fallback_.read_mutex_))
    return -1;

  if (uv_mutex_init(&rwlock->fallback_.write_mutex_)) {
    uv_mutex_destroy(&rwlock->fallback_.read_mutex_);
    return -1;
  }

  rwlock->fallback_.num_readers_ = 0;

  return 0;
}


inline static void uv__rwlock_fallback_destroy(uv_rwlock_t* rwlock) {
  uv_mutex_destroy(&rwlock->fallback_.read_mutex_);
  uv_mutex_destroy(&rwlock->fallback_.write_mutex_);
}


inline static void uv__rwlock_fallback_rdlock(uv_rwlock_t* rwlock) {
  uv_mutex_lock(&rwlock->fallback_.read_mutex_);

  if (++rwlock->fallback_.num_readers_ == 1)
    uv_mutex_lock(&rwlock->fallback_.write_mutex_);

  uv_mutex_unlock(&rwlock->fallback_.read_mutex_);
}


inline static int uv__rwlock_fallback_tryrdlock(uv_rwlock_t* rwlock) {
  int ret;

  ret = -1;

  if (uv_mutex_trylock(&rwlock->fallback_.read_mutex_))
    goto out;

  if (rwlock->fallback_.num_readers_ == 0)
    ret = uv_mutex_trylock(&rwlock->fallback_.write_mutex_);
  else
    ret = 0;

  if (ret == 0)
    rwlock->fallback_.num_readers_++;

  uv_mutex_unlock(&rwlock->fallback_.read_mutex_);

out:
  return ret;
}


inline static void uv__rwlock_fallback_rdunlock(uv_rwlock_t* rwlock) {
  uv_mutex_lock(&rwlock->fallback_.read_mutex_);

  if (--rwlock->fallback_.num_readers_ == 0)
    uv_mutex_unlock(&rwlock->fallback_.write_mutex_);

  uv_mutex_unlock(&rwlock->fallback_.read_mutex_);
}


inline static void uv__rwlock_fallback_wrlock(uv_rwlock_t* rwlock) {
  uv_mutex_lock(&rwlock->fallback_.write_mutex_);
}


inline static int uv__rwlock_fallback_trywrlock(uv_rwlock_t* rwlock) {
  return uv_mutex_trylock(&rwlock->fallback_.write_mutex_);
}


inline static void uv__rwlock_fallback_wrunlock(uv_rwlock_t* rwlock) {
  uv_mutex_unlock(&rwlock->fallback_.write_mutex_);
}
