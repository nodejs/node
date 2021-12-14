/*
 * Copyright 2016-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if defined(_WIN32)
# include <windows.h>
# if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x600
#  define USE_RWLOCK
# endif
#endif

#include <openssl/crypto.h>

#if defined(OPENSSL_THREADS) && !defined(CRYPTO_TDEBUG) && defined(OPENSSL_SYS_WINDOWS)

# ifdef USE_RWLOCK
typedef struct {
    SRWLOCK lock;
    int exclusive;
} CRYPTO_win_rwlock;
# endif

CRYPTO_RWLOCK *CRYPTO_THREAD_lock_new(void)
{
    CRYPTO_RWLOCK *lock;
# ifdef USE_RWLOCK
    CRYPTO_win_rwlock *rwlock;

    if ((lock = OPENSSL_zalloc(sizeof(CRYPTO_win_rwlock))) == NULL)
        return NULL;
    rwlock = lock;
    InitializeSRWLock(&rwlock->lock);
# else

    if ((lock = OPENSSL_zalloc(sizeof(CRITICAL_SECTION))) == NULL) {
        /* Don't set error, to avoid recursion blowup. */
        return NULL;
    }

#  if !defined(_WIN32_WCE)
    /* 0x400 is the spin count value suggested in the documentation */
    if (!InitializeCriticalSectionAndSpinCount(lock, 0x400)) {
        OPENSSL_free(lock);
        return NULL;
    }
#  else
    InitializeCriticalSection(lock);
#  endif
# endif

    return lock;
}

__owur int CRYPTO_THREAD_read_lock(CRYPTO_RWLOCK *lock)
{
# ifdef USE_RWLOCK
    CRYPTO_win_rwlock *rwlock = lock;

    AcquireSRWLockShared(&rwlock->lock);
# else
    EnterCriticalSection(lock);
# endif
    return 1;
}

__owur int CRYPTO_THREAD_write_lock(CRYPTO_RWLOCK *lock)
{
# ifdef USE_RWLOCK
    CRYPTO_win_rwlock *rwlock = lock;

    AcquireSRWLockExclusive(&rwlock->lock);
    rwlock->exclusive = 1;
# else
    EnterCriticalSection(lock);
# endif
    return 1;
}

int CRYPTO_THREAD_unlock(CRYPTO_RWLOCK *lock)
{
# ifdef USE_RWLOCK
    CRYPTO_win_rwlock *rwlock = lock;

    if (rwlock->exclusive) {
        rwlock->exclusive = 0;
        ReleaseSRWLockExclusive(&rwlock->lock);
    } else {
        ReleaseSRWLockShared(&rwlock->lock);
    }
# else
    LeaveCriticalSection(lock);
# endif
    return 1;
}

void CRYPTO_THREAD_lock_free(CRYPTO_RWLOCK *lock)
{
    if (lock == NULL)
        return;

# ifndef USE_RWLOCK
    DeleteCriticalSection(lock);
# endif
    OPENSSL_free(lock);

    return;
}

# define ONCE_UNINITED     0
# define ONCE_ININIT       1
# define ONCE_DONE         2

/*
 * We don't use InitOnceExecuteOnce because that isn't available in WinXP which
 * we still have to support.
 */
int CRYPTO_THREAD_run_once(CRYPTO_ONCE *once, void (*init)(void))
{
    LONG volatile *lock = (LONG *)once;
    LONG result;

    if (*lock == ONCE_DONE)
        return 1;

    do {
        result = InterlockedCompareExchange(lock, ONCE_ININIT, ONCE_UNINITED);
        if (result == ONCE_UNINITED) {
            init();
            *lock = ONCE_DONE;
            return 1;
        }
    } while (result == ONCE_ININIT);

    return (*lock == ONCE_DONE);
}

int CRYPTO_THREAD_init_local(CRYPTO_THREAD_LOCAL *key, void (*cleanup)(void *))
{
    *key = TlsAlloc();
    if (*key == TLS_OUT_OF_INDEXES)
        return 0;

    return 1;
}

void *CRYPTO_THREAD_get_local(CRYPTO_THREAD_LOCAL *key)
{
    DWORD last_error;
    void *ret;

    /*
     * TlsGetValue clears the last error even on success, so that callers may
     * distinguish it successfully returning NULL or failing. It is documented
     * to never fail if the argument is a valid index from TlsAlloc, so we do
     * not need to handle this.
     *
     * However, this error-mangling behavior interferes with the caller's use of
     * GetLastError. In particular SSL_get_error queries the error queue to
     * determine whether the caller should look at the OS's errors. To avoid
     * destroying state, save and restore the Windows error.
     *
     * https://msdn.microsoft.com/en-us/library/windows/desktop/ms686812(v=vs.85).aspx
     */
    last_error = GetLastError();
    ret = TlsGetValue(*key);
    SetLastError(last_error);
    return ret;
}

int CRYPTO_THREAD_set_local(CRYPTO_THREAD_LOCAL *key, void *val)
{
    if (TlsSetValue(*key, val) == 0)
        return 0;

    return 1;
}

int CRYPTO_THREAD_cleanup_local(CRYPTO_THREAD_LOCAL *key)
{
    if (TlsFree(*key) == 0)
        return 0;

    return 1;
}

CRYPTO_THREAD_ID CRYPTO_THREAD_get_current_id(void)
{
    return GetCurrentThreadId();
}

int CRYPTO_THREAD_compare_id(CRYPTO_THREAD_ID a, CRYPTO_THREAD_ID b)
{
    return (a == b);
}

int CRYPTO_atomic_add(int *val, int amount, int *ret, CRYPTO_RWLOCK *lock)
{
    *ret = (int)InterlockedExchangeAdd((long volatile *)val, (long)amount) + amount;
    return 1;
}

int CRYPTO_atomic_or(uint64_t *val, uint64_t op, uint64_t *ret,
                     CRYPTO_RWLOCK *lock)
{
    *ret = (uint64_t)InterlockedOr64((LONG64 volatile *)val, (LONG64)op) | op;
    return 1;
}

int CRYPTO_atomic_load(uint64_t *val, uint64_t *ret, CRYPTO_RWLOCK *lock)
{
    *ret = (uint64_t)InterlockedOr64((LONG64 volatile *)val, 0);
    return 1;
}

int openssl_init_fork_handlers(void)
{
    return 0;
}

int openssl_get_fork_id(void)
{
    return 0;
}
#endif
