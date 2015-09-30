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
#include <stdlib.h>

#include "uv.h"
#include "internal.h"


#define HAVE_CONDVAR_API() (pInitializeConditionVariable != NULL)

static int uv_cond_fallback_init(uv_cond_t* cond);
static void uv_cond_fallback_destroy(uv_cond_t* cond);
static void uv_cond_fallback_signal(uv_cond_t* cond);
static void uv_cond_fallback_broadcast(uv_cond_t* cond);
static void uv_cond_fallback_wait(uv_cond_t* cond, uv_mutex_t* mutex);
static int uv_cond_fallback_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout);

static int uv_cond_condvar_init(uv_cond_t* cond);
static void uv_cond_condvar_destroy(uv_cond_t* cond);
static void uv_cond_condvar_signal(uv_cond_t* cond);
static void uv_cond_condvar_broadcast(uv_cond_t* cond);
static void uv_cond_condvar_wait(uv_cond_t* cond, uv_mutex_t* mutex);
static int uv_cond_condvar_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout);


static void uv__once_inner(uv_once_t* guard, void (*callback)(void)) {
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


/* Verify that uv_thread_t can be stored in a TLS slot. */
STATIC_ASSERT(sizeof(uv_thread_t) <= sizeof(void*));

static uv_key_t uv__current_thread_key;
static uv_once_t uv__current_thread_init_guard = UV_ONCE_INIT;


static void uv__init_current_thread_key(void) {
  if (uv_key_create(&uv__current_thread_key))
    abort();
}


struct thread_ctx {
  void (*entry)(void* arg);
  void* arg;
  uv_thread_t self;
};


static UINT __stdcall uv__thread_start(void* arg) {
  struct thread_ctx *ctx_p;
  struct thread_ctx ctx;

  ctx_p = arg;
  ctx = *ctx_p;
  uv__free(ctx_p);

  uv_once(&uv__current_thread_init_guard, uv__init_current_thread_key);
  uv_key_set(&uv__current_thread_key, (void*) ctx.self);

  ctx.entry(ctx.arg);

  return 0;
}


int uv_thread_create(uv_thread_t *tid, void (*entry)(void *arg), void *arg) {
  struct thread_ctx* ctx;
  int err;
  HANDLE thread;

  ctx = uv__malloc(sizeof(*ctx));
  if (ctx == NULL)
    return UV_ENOMEM;

  ctx->entry = entry;
  ctx->arg = arg;

  /* Create the thread in suspended state so we have a chance to pass
   * its own creation handle to it */   
  thread = (HANDLE) _beginthreadex(NULL,
                                   0,
                                   uv__thread_start,
                                   ctx,
                                   CREATE_SUSPENDED,
                                   NULL);
  if (thread == NULL) {
    err = errno;
    uv__free(ctx);
  } else {
    err = 0;
    *tid = thread;
    ctx->self = thread;
    ResumeThread(thread);
  }

  switch (err) {
    case 0:
      return 0;
    case EACCES:
      return UV_EACCES;
    case EAGAIN:
      return UV_EAGAIN;
    case EINVAL:
      return UV_EINVAL;
  }

  return UV_EIO;
}


uv_thread_t uv_thread_self(void) {
  uv_once(&uv__current_thread_init_guard, uv__init_current_thread_key);
  return (uv_thread_t) uv_key_get(&uv__current_thread_key);
}


int uv_thread_join(uv_thread_t *tid) {
  if (WaitForSingleObject(*tid, INFINITE))
    return uv_translate_sys_error(GetLastError());
  else {
    CloseHandle(*tid);
    *tid = 0;
    return 0;
  }
}


int uv_thread_equal(const uv_thread_t* t1, const uv_thread_t* t2) {
  return *t1 == *t2;
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
    return UV_EBUSY;
}


void uv_mutex_unlock(uv_mutex_t* mutex) {
  LeaveCriticalSection(mutex);
}


int uv_rwlock_init(uv_rwlock_t* rwlock) {
  /* Initialize the semaphore that acts as the write lock. */
  HANDLE handle = CreateSemaphoreW(NULL, 1, 1, NULL);
  if (handle == NULL)
    return uv_translate_sys_error(GetLastError());
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
    return UV_EBUSY;

  err = 0;

  if (rwlock->state_.num_readers_ == 0) {
    /* Currently there are no other readers, which means that the write lock
     * needs to be acquired.
     */
    DWORD r = WaitForSingleObject(rwlock->state_.write_semaphore_, 0);
    if (r == WAIT_OBJECT_0)
      rwlock->state_.num_readers_++;
    else if (r == WAIT_TIMEOUT)
      err = UV_EBUSY;
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
    return UV_EBUSY;
  else
    uv_fatal_error(GetLastError(), "WaitForSingleObject");
}


void uv_rwlock_wrunlock(uv_rwlock_t* rwlock) {
  if (!ReleaseSemaphore(rwlock->state_.write_semaphore_, 1, NULL))
    uv_fatal_error(GetLastError(), "ReleaseSemaphore");
}


int uv_sem_init(uv_sem_t* sem, unsigned int value) {
  *sem = CreateSemaphore(NULL, value, INT_MAX, NULL);
  if (*sem == NULL)
    return uv_translate_sys_error(GetLastError());
  else
    return 0;
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
    return UV_EAGAIN;

  abort();
  return -1; /* Satisfy the compiler. */
}


/* This condition variable implementation is based on the SetEvent solution
 * (section 3.2) at http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
 * We could not use the SignalObjectAndWait solution (section 3.4) because
 * it want the 2nd argument (type uv_mutex_t) of uv_cond_wait() and
 * uv_cond_timedwait() to be HANDLEs, but we use CRITICAL_SECTIONs.
 */

static int uv_cond_fallback_init(uv_cond_t* cond) {
  int err;

  /* Initialize the count to 0. */
  cond->fallback.waiters_count = 0;

  InitializeCriticalSection(&cond->fallback.waiters_count_lock);

  /* Create an auto-reset event. */
  cond->fallback.signal_event = CreateEvent(NULL,  /* no security */
                                            FALSE, /* auto-reset event */
                                            FALSE, /* non-signaled initially */
                                            NULL); /* unnamed */
  if (!cond->fallback.signal_event) {
    err = GetLastError();
    goto error2;
  }

  /* Create a manual-reset event. */
  cond->fallback.broadcast_event = CreateEvent(NULL,  /* no security */
                                               TRUE,  /* manual-reset */
                                               FALSE, /* non-signaled */
                                               NULL); /* unnamed */
  if (!cond->fallback.broadcast_event) {
    err = GetLastError();
    goto error;
  }

  return 0;

error:
  CloseHandle(cond->fallback.signal_event);
error2:
  DeleteCriticalSection(&cond->fallback.waiters_count_lock);
  return uv_translate_sys_error(err);
}


static int uv_cond_condvar_init(uv_cond_t* cond) {
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


static void uv_cond_fallback_destroy(uv_cond_t* cond) {
  if (!CloseHandle(cond->fallback.broadcast_event))
    abort();
  if (!CloseHandle(cond->fallback.signal_event))
    abort();
  DeleteCriticalSection(&cond->fallback.waiters_count_lock);
}


static void uv_cond_condvar_destroy(uv_cond_t* cond) {
  /* nothing to do */
}


void uv_cond_destroy(uv_cond_t* cond) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_destroy(cond);
  else
    uv_cond_fallback_destroy(cond);
}


static void uv_cond_fallback_signal(uv_cond_t* cond) {
  int have_waiters;

  /* Avoid race conditions. */
  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  have_waiters = cond->fallback.waiters_count > 0;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  if (have_waiters)
    SetEvent(cond->fallback.signal_event);
}


static void uv_cond_condvar_signal(uv_cond_t* cond) {
  pWakeConditionVariable(&cond->cond_var);
}


void uv_cond_signal(uv_cond_t* cond) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_signal(cond);
  else
    uv_cond_fallback_signal(cond);
}


static void uv_cond_fallback_broadcast(uv_cond_t* cond) {
  int have_waiters;

  /* Avoid race conditions. */
  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  have_waiters = cond->fallback.waiters_count > 0;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  if (have_waiters)
    SetEvent(cond->fallback.broadcast_event);
}


static void uv_cond_condvar_broadcast(uv_cond_t* cond) {
  pWakeAllConditionVariable(&cond->cond_var);
}


void uv_cond_broadcast(uv_cond_t* cond) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_broadcast(cond);
  else
    uv_cond_fallback_broadcast(cond);
}


static int uv_cond_wait_helper(uv_cond_t* cond, uv_mutex_t* mutex,
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
    return UV_ETIMEDOUT;

  abort();
  return -1; /* Satisfy the compiler. */
}


static void uv_cond_fallback_wait(uv_cond_t* cond, uv_mutex_t* mutex) {
  if (uv_cond_wait_helper(cond, mutex, INFINITE))
    abort();
}


static void uv_cond_condvar_wait(uv_cond_t* cond, uv_mutex_t* mutex) {
  if (!pSleepConditionVariableCS(&cond->cond_var, mutex, INFINITE))
    abort();
}


void uv_cond_wait(uv_cond_t* cond, uv_mutex_t* mutex) {
  if (HAVE_CONDVAR_API())
    uv_cond_condvar_wait(cond, mutex);
  else
    uv_cond_fallback_wait(cond, mutex);
}


static int uv_cond_fallback_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout) {
  return uv_cond_wait_helper(cond, mutex, (DWORD)(timeout / 1e6));
}


static int uv_cond_condvar_timedwait(uv_cond_t* cond,
    uv_mutex_t* mutex, uint64_t timeout) {
  if (pSleepConditionVariableCS(&cond->cond_var, mutex, (DWORD)(timeout / 1e6)))
    return 0;
  if (GetLastError() != ERROR_TIMEOUT)
    abort();
  return UV_ETIMEDOUT;
}


int uv_cond_timedwait(uv_cond_t* cond, uv_mutex_t* mutex,
    uint64_t timeout) {
  if (HAVE_CONDVAR_API())
    return uv_cond_condvar_timedwait(cond, mutex, timeout);
  else
    return uv_cond_fallback_timedwait(cond, mutex, timeout);
}


int uv_barrier_init(uv_barrier_t* barrier, unsigned int count) {
  int err;

  barrier->n = count;
  barrier->count = 0;

  err = uv_mutex_init(&barrier->mutex);
  if (err)
    return err;

  err = uv_sem_init(&barrier->turnstile1, 0);
  if (err)
    goto error2;

  err = uv_sem_init(&barrier->turnstile2, 1);
  if (err)
    goto error;

  return 0;

error:
  uv_sem_destroy(&barrier->turnstile1);
error2:
  uv_mutex_destroy(&barrier->mutex);
  return err;

}


void uv_barrier_destroy(uv_barrier_t* barrier) {
  uv_sem_destroy(&barrier->turnstile2);
  uv_sem_destroy(&barrier->turnstile1);
  uv_mutex_destroy(&barrier->mutex);
}


int uv_barrier_wait(uv_barrier_t* barrier) {
  int serial_thread;

  uv_mutex_lock(&barrier->mutex);
  if (++barrier->count == barrier->n) {
    uv_sem_wait(&barrier->turnstile2);
    uv_sem_post(&barrier->turnstile1);
  }
  uv_mutex_unlock(&barrier->mutex);

  uv_sem_wait(&barrier->turnstile1);
  uv_sem_post(&barrier->turnstile1);

  uv_mutex_lock(&barrier->mutex);
  serial_thread = (--barrier->count == 0);
  if (serial_thread) {
    uv_sem_wait(&barrier->turnstile1);
    uv_sem_post(&barrier->turnstile2);
  }
  uv_mutex_unlock(&barrier->mutex);

  uv_sem_wait(&barrier->turnstile2);
  uv_sem_post(&barrier->turnstile2);
  return serial_thread;
}


int uv_key_create(uv_key_t* key) {
  key->tls_index = TlsAlloc();
  if (key->tls_index == TLS_OUT_OF_INDEXES)
    return UV_ENOMEM;
  return 0;
}


void uv_key_delete(uv_key_t* key) {
  if (TlsFree(key->tls_index) == FALSE)
    abort();
  key->tls_index = TLS_OUT_OF_INDEXES;
}


void* uv_key_get(uv_key_t* key) {
  void* value;

  value = TlsGetValue(key->tls_index);
  if (value == NULL)
    if (GetLastError() != ERROR_SUCCESS)
      abort();

  return value;
}


void uv_key_set(uv_key_t* key, void* value) {
  if (TlsSetValue(key->tls_index, value) == FALSE)
    abort();
}
