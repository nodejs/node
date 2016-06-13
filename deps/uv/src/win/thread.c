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

#include <assert.h>
#include <limits.h>

#include "uv.h"
#include "internal.h"


#define HAVE_CONDVAR_API() (pInitializeConditionVariable != NULL)

#ifdef _MSC_VER /* msvc */
# define inline __inline
# define NOINLINE __declspec (noinline)
#else  /* gcc */
# define inline inline
# define NOINLINE __attribute__ ((noinline))
#endif


inline static int uv_cond_fallback_init(uv_cond_t* cond);
inline static void uv_cond_fallback_destroy(uv_cond_t* cond);
inline static void uv_cond_fallback_signal(uv_cond_t* cond);
inline static void uv_cond_fallback_broadcast(uv_cond_t* cond);
inline static void uv_cond_fallback_wait(uv_cond_t* cond, uv_mutex_t* mutex);
inline static int uv_cond_fallback_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout);

inline static int uv_cond_condvar_init(uv_cond_t* cond);
inline static void uv_cond_condvar_destroy(uv_cond_t* cond);
inline static void uv_cond_condvar_signal(uv_cond_t* cond);
inline static void uv_cond_condvar_broadcast(uv_cond_t* cond);
inline static void uv_cond_condvar_wait(uv_cond_t* cond, uv_mutex_t* mutex);
inline static int uv_cond_condvar_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout);


static NOINLINE void uv__once_inner(uv_once_t* guard,
    void (*callback)(void)) {
  DWORD result;
  HANDLE existing_event, created_event;

  created_event = CreateEvent(NULL, 1, 0, NULL);
  if (created_event == 0) {
    /* Could fail in a low-memory situation? */
    uv_fatal_error(GetLastError(), "CreateEvent");
  }

  existing_event = InterlockedCompareExchangePointer(&guard->event,
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
    /* existing one todv become signaled. */
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
  /* Initialize the semaphore that acts as the write lock. */
  HANDLE handle = CreateSemaphoreW(NULL, 1, 1, NULL);
  if (handle == NULL)
    return -1;
  rwlock->state_.write_semaphore_ = handle;

  /* Initialize the critical section protecting the reader count. */
  InitializeCriticalSection(&rwlock->state_.num_readers_lock_);

  /* Initialize the reader count. */
  rwlock->state_.num_readers_ = 0;

  return 0;
}


void uv_rwlock_destroy(uv_rwlock_t* rwlock) {
  DeleteCriticalSection(&rwlock->state_.num_readers_lock_);
  CloseHandle(rwlock->state_.write_semaphore_);
}


void uv_rwlock_rdlock(uv_rwlock_t* rwlock) {
  /* Acquire the lock that protects the reader count. */
  EnterCriticalSection(&rwlock->state_.num_readers_lock_);

  /* Increase the reader count, and lock for write if this is the first
   * reader.
   */
  if (++rwlock->state_.num_readers_ == 1) {
    DWORD r = WaitForSingleObject(rwlock->state_.write_semaphore_, INFINITE);
    if (r != WAIT_OBJECT_0)
      uv_fatal_error(GetLastError(), "WaitForSingleObject");
  }

  /* Release the lock that protects the reader count. */
  LeaveCriticalSection(&rwlock->state_.num_readers_lock_);
}


int uv_rwlock_tryrdlock(uv_rwlock_t* rwlock) {
  int err;

  if (!TryEnterCriticalSection(&rwlock->state_.num_readers_lock_))
    return -1;

  err = 0;

  if (rwlock->state_.num_readers_ == 0) {
    /* Currently there are no other readers, which means that the write lock
     * needs to be acquired.
     */
    DWORD r = WaitForSingleObject(rwlock->state_.write_semaphore_, 0);
    if (r == WAIT_OBJECT_0)
      rwlock->state_.num_readers_++;
    else if (r == WAIT_TIMEOUT)
      err = -1;
    else if (r == WAIT_FAILED)
      uv_fatal_error(GetLastError(), "WaitForSingleObject");

  } else {
    /* The write lock has already been acquired because there are other
     * active readers.
     */
    rwlock->state_.num_readers_++;
  }

  LeaveCriticalSection(&rwlock->state_.num_readers_lock_);
  return err;
}


void uv_rwlock_rdunlock(uv_rwlock_t* rwlock) {
  EnterCriticalSection(&rwlock->state_.num_readers_lock_);

  if (--rwlock->state_.num_readers_ == 0) {
    if (!ReleaseSemaphore(rwlock->state_.write_semaphore_, 1, NULL))
      uv_fatal_error(GetLastError(), "ReleaseSemaphore");
  }

  LeaveCriticalSection(&rwlock->state_.num_readers_lock_);
}


void uv_rwlock_wrlock(uv_rwlock_t* rwlock) {
  DWORD r = WaitForSingleObject(rwlock->state_.write_semaphore_, INFINITE);
  if (r != WAIT_OBJECT_0)
    uv_fatal_error(GetLastError(), "WaitForSingleObject");
}


int uv_rwlock_trywrlock(uv_rwlock_t* rwlock) {
  DWORD r = WaitForSingleObject(rwlock->state_.write_semaphore_, 0);
  if (r == WAIT_OBJECT_0)
    return 0;
  else if (r == WAIT_TIMEOUT)
    return -1;
  else
    uv_fatal_error(GetLastError(), "WaitForSingleObject");
  return -1;
}


void uv_rwlock_wrunlock(uv_rwlock_t* rwlock) {
  if (!ReleaseSemaphore(rwlock->state_.write_semaphore_, 1, NULL))
    uv_fatal_error(GetLastError(), "ReleaseSemaphore");
}


int uv_sem_init(uv_sem_t* sem, unsigned int value) {
  *sem = CreateSemaphore(NULL, value, INT_MAX, NULL);
  return *sem ? 0 : -1;
}


void uv_sem_destroy(uv_sem_t* sem) {
  if (!CloseHandle(*sem))
    abort();
}


void uv_sem_post(uv_sem_t* sem) {
  if (!ReleaseSemaphore(*sem, 1, NULL))
    abort();
}


void uv_sem_wait(uv_sem_t* sem) {
  if (WaitForSingleObject(*sem, INFINITE) != WAIT_OBJECT_0)
    abort();
}


int uv_sem_trywait(uv_sem_t* sem) {
  DWORD r = WaitForSingleObject(*sem, 0);

  if (r == WAIT_OBJECT_0)
    return 0;

  if (r == WAIT_TIMEOUT)
    return -1;

  abort();
  return -1; /* Satisfy the compiler. */
}


/* This condition variable implementation is based on the SetEvent solution
 * (section 3.2) at http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
 * We could not use the SignalObjectAndWait solution (section 3.4) because
 * it want the 2nd argument (type uv_mutex_t) of uv_cond_wait() and
 * uv_cond_timedwait() to be HANDLEs, but we use CRITICAL_SECTIONs.
 */

inline static int uv_cond_fallback_init(uv_cond_t* cond) {
  /* Initialize the count to 0. */
  cond->fallback.waiters_count = 0;

  InitializeCriticalSection(&cond->fallback.waiters_count_lock);

  /* Create an auto-reset event. */
  cond->fallback.signal_event = CreateEvent(NULL,  /* no security */
                                            FALSE, /* auto-reset event */
                                            FALSE, /* non-signaled initially */
                                            NULL); /* unnamed */
  if (!cond->fallback.signal_event)
    goto error2;

  /* Create a manual-reset event. */
  cond->fallback.broadcast_event = CreateEvent(NULL,  /* no security */
                                               TRUE,  /* manual-reset */
                                               FALSE, /* non-signaled */
                                               NULL); /* unnamed */
  if (!cond->fallback.broadcast_event)
    goto error;

  return 0;

error:
  CloseHandle(cond->fallback.signal_event);
error2:
  DeleteCriticalSection(&cond->fallback.waiters_count_lock);
  return -1;
}


inline static int uv_cond_condvar_init(uv_cond_t* cond) {
  pInitializeConditionVariable(&cond->cond_var);
  return 0;
}


int uv_cond_init(uv_cond_t* cond) {
  uv__once_init();

  if (HAVE_CONDVAR_API())
    return uv_cond_condvar_init(cond);
  else
    return uv_cond_fallback_init(cond);
}


inline static void uv_cond_fallback_destroy(uv_cond_t* cond) {
  if (!CloseHandle(cond->fallback.broadcast_event))
    abort();
  if (!CloseHandle(cond->fallback.signal_event))
    abort();
  DeleteCriticalSection(&cond->fallback.waiters_count_lock);
}


inline static void uv_cond_condvar_destroy(uv_cond_t* cond) {
  /* nothing to do */
}


void uv_cond_destroy(uv_cond_t* cond) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_destroy(cond);
  else
    uv_cond_fallback_destroy(cond);
}


inline static void uv_cond_fallback_signal(uv_cond_t* cond) {
  int have_waiters;

  /* Avoid race conditions. */
  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  have_waiters = cond->fallback.waiters_count > 0;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  if (have_waiters)
    SetEvent(cond->fallback.signal_event);
}


inline static void uv_cond_condvar_signal(uv_cond_t* cond) {
  pWakeConditionVariable(&cond->cond_var);
}


void uv_cond_signal(uv_cond_t* cond) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_signal(cond);
  else
    uv_cond_fallback_signal(cond);
}


inline static void uv_cond_fallback_broadcast(uv_cond_t* cond) {
  int have_waiters;

  /* Avoid race conditions. */
  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  have_waiters = cond->fallback.waiters_count > 0;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  if (have_waiters)
    SetEvent(cond->fallback.broadcast_event);
}


inline static void uv_cond_condvar_broadcast(uv_cond_t* cond) {
  pWakeAllConditionVariable(&cond->cond_var);
}


void uv_cond_broadcast(uv_cond_t* cond) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_broadcast(cond);
  else
    uv_cond_fallback_broadcast(cond);
}


inline int uv_cond_wait_helper(uv_cond_t* cond, uv_mutex_t* mutex,
    DWORD dwMilliseconds) {
  DWORD result;
  int last_waiter;
  HANDLE handles[2] = {
    cond->fallback.signal_event,
    cond->fallback.broadcast_event
  };

  /* Avoid race conditions. */
  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  cond->fallback.waiters_count++;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  /* It's ok to release the <mutex> here since Win32 manual-reset events */
  /* maintain state when used with <SetEvent>. This avoids the "lost wakeup" */
  /* bug. */
  uv_mutex_unlock(mutex);

  /* Wait for either event to become signaled due to <uv_cond_signal> being */
  /* called or <uv_cond_broadcast> being called. */
  result = WaitForMultipleObjects(2, handles, FALSE, dwMilliseconds);

  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  cond->fallback.waiters_count--;
  last_waiter = result == WAIT_OBJECT_0 + 1
      && cond->fallback.waiters_count == 0;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  /* Some thread called <pthread_cond_broadcast>. */
  if (last_waiter) {
    /* We're the last waiter to be notified or to stop waiting, so reset the */
    /* the manual-reset event. */
    ResetEvent(cond->fallback.broadcast_event);
  }

  /* Reacquire the <mutex>. */
  uv_mutex_lock(mutex);

  if (result == WAIT_OBJECT_0 || result == WAIT_OBJECT_0 + 1)
    return 0;

  if (result == WAIT_TIMEOUT)
    return -1;

  abort();
  return -1; /* Satisfy the compiler. */
}


inline static void uv_cond_fallback_wait(uv_cond_t* cond, uv_mutex_t* mutex) {
  if (uv_cond_wait_helper(cond, mutex, INFINITE))
    abort();
}


inline static void uv_cond_condvar_wait(uv_cond_t* cond, uv_mutex_t* mutex) {
  if (!pSleepConditionVariableCS(&cond->cond_var, mutex, INFINITE))
    abort();
}


void uv_cond_wait(uv_cond_t* cond, uv_mutex_t* mutex) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_wait(cond, mutex);
  else
    uv_cond_fallback_wait(cond, mutex);
}


inline static int uv_cond_fallback_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout) {
  return uv_cond_wait_helper(cond, mutex, (DWORD)(timeout / 1e6));
}


inline static int uv_cond_condvar_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout) {
  if (pSleepConditionVariableCS(&cond->cond_var, mutex, (DWORD)(timeout / 1e6)))
    return 0;
  if (GetLastError() != ERROR_TIMEOUT)
    abort();
  return -1;
}


int uv_cond_timedwait(uv_cond_t* cond, uv_mutex_t* mutex,
    uint64_t timeout) {
  if (HAVE_CONDVAR_API())
    return uv_cond_condvar_timedwait(cond, mutex, timeout);
  else
    return uv_cond_fallback_timedwait(cond, mutex, timeout);
}


int uv_barrier_init(uv_barrier_t* barrier, unsigned int count) {
  barrier->n = count;
  barrier->count = 0;

  if (uv_mutex_init(&barrier->mutex))
    return -1;

  if (uv_sem_init(&barrier->turnstile1, 0))
    goto error2;

  if (uv_sem_init(&barrier->turnstile2, 1))
    goto error;

  return 0;

error:
  uv_sem_destroy(&barrier->turnstile1);
error2:
  uv_mutex_destroy(&barrier->mutex);
  return -1;

}


void uv_barrier_destroy(uv_barrier_t* barrier) {
  uv_sem_destroy(&barrier->turnstile2);
  uv_sem_destroy(&barrier->turnstile1);
  uv_mutex_destroy(&barrier->mutex);
}


void uv_barrier_wait(uv_barrier_t* barrier) {
  uv_mutex_lock(&barrier->mutex);
  if (++barrier->count == barrier->n) {
    uv_sem_wait(&barrier->turnstile2);
    uv_sem_post(&barrier->turnstile1);
  }
  uv_mutex_unlock(&barrier->mutex);

  uv_sem_wait(&barrier->turnstile1);
  uv_sem_post(&barrier->turnstile1);

  uv_mutex_lock(&barrier->mutex);
  if (--barrier->count == 0) {
    uv_sem_wait(&barrier->turnstile1);
    uv_sem_post(&barrier->turnstile2);
  }
  uv_mutex_unlock(&barrier->mutex);

  uv_sem_wait(&barrier->turnstile2);
  uv_sem_post(&barrier->turnstile2);
}
