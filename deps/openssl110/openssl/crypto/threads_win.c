/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if defined(_WIN32)
# include <windows.h>
#endif

#include <openssl/crypto.h>

#if defined(OPENSSL_THREADS) && !defined(CRYPTO_TDEBUG) && defined(OPENSSL_SYS_WINDOWS)

CRYPTO_RWLOCK *CRYPTO_THREAD_lock_new(void)
{
    CRYPTO_RWLOCK *lock = OPENSSL_zalloc(sizeof(CRITICAL_SECTION));
    if (lock == NULL)
        return NULL;

    /* 0x400 is the spin count value suggested in the documentation */
    if (!InitializeCriticalSectionAndSpinCount(lock, 0x400)) {
        OPENSSL_free(lock);
        return NULL;
    }

    return lock;
}

int CRYPTO_THREAD_read_lock(CRYPTO_RWLOCK *lock)
{
    EnterCriticalSection(lock);
    return 1;
}

int CRYPTO_THREAD_write_lock(CRYPTO_RWLOCK *lock)
{
    EnterCriticalSection(lock);
    return 1;
}

int CRYPTO_THREAD_unlock(CRYPTO_RWLOCK *lock)
{
    LeaveCriticalSection(lock);
    return 1;
}

void CRYPTO_THREAD_lock_free(CRYPTO_RWLOCK *lock)
{
    if (lock == NULL)
        return;

    DeleteCriticalSection(lock);
    OPENSSL_free(lock);

    return;
}

#  define ONCE_UNINITED     0
#  define ONCE_ININIT       1
#  define ONCE_DONE         2

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
    return TlsGetValue(*key);
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
    *ret = InterlockedExchangeAdd(val, amount) + amount;
    return 1;
}

#endif
