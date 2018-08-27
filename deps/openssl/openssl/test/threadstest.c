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

#include <stdio.h>

#include <openssl/crypto.h>

#if !defined(OPENSSL_THREADS) || defined(CRYPTO_TDEBUG)

typedef unsigned int thread_t;

static int run_thread(thread_t *t, void (*f)(void))
{
    f();
    return 1;
}

static int wait_for_thread(thread_t thread)
{
    return 1;
}

#elif defined(OPENSSL_SYS_WINDOWS)

typedef HANDLE thread_t;

static DWORD WINAPI thread_run(LPVOID arg)
{
    void (*f)(void);

    *(void **) (&f) = arg;

    f();
    return 0;
}

static int run_thread(thread_t *t, void (*f)(void))
{
    *t = CreateThread(NULL, 0, thread_run, *(void **) &f, 0, NULL);
    return *t != NULL;
}

static int wait_for_thread(thread_t thread)
{
    return WaitForSingleObject(thread, INFINITE) == 0;
}

#else

typedef pthread_t thread_t;

static void *thread_run(void *arg)
{
    void (*f)(void);

    *(void **) (&f) = arg;

    f();
    return NULL;
}

static int run_thread(thread_t *t, void (*f)(void))
{
    return pthread_create(t, NULL, thread_run, *(void **) &f) == 0;
}

static int wait_for_thread(thread_t thread)
{
    return pthread_join(thread, NULL) == 0;
}

#endif

static int test_lock(void)
{
    CRYPTO_RWLOCK *lock = CRYPTO_THREAD_lock_new();

    if (!CRYPTO_THREAD_read_lock(lock)) {
        fprintf(stderr, "CRYPTO_THREAD_read_lock() failed\n");
        return 0;
    }

    if (!CRYPTO_THREAD_unlock(lock)) {
        fprintf(stderr, "CRYPTO_THREAD_unlock() failed\n");
        return 0;
    }

    CRYPTO_THREAD_lock_free(lock);

    return 1;
}

static CRYPTO_ONCE once_run = CRYPTO_ONCE_STATIC_INIT;
static unsigned once_run_count = 0;

static void once_do_run(void)
{
    once_run_count++;
}

static void once_run_thread_cb(void)
{
    CRYPTO_THREAD_run_once(&once_run, once_do_run);
}

static int test_once(void)
{
    thread_t thread;
    if (!run_thread(&thread, once_run_thread_cb) ||
        !wait_for_thread(thread))
    {
        fprintf(stderr, "run_thread() failed\n");
        return 0;
    }

    if (!CRYPTO_THREAD_run_once(&once_run, once_do_run)) {
        fprintf(stderr, "CRYPTO_THREAD_run_once() failed\n");
        return 0;
    }

    if (once_run_count != 1) {
        fprintf(stderr, "once run %u times\n", once_run_count);
        return 0;
    }

    return 1;
}

static CRYPTO_THREAD_LOCAL thread_local_key;
static unsigned destructor_run_count = 0;
static int thread_local_thread_cb_ok = 0;

static void thread_local_destructor(void *arg)
{
    unsigned *count;

    if (arg == NULL)
        return;

    count = arg;

    (*count)++;
}

static void thread_local_thread_cb(void)
{
    void *ptr;

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (ptr != NULL) {
        fprintf(stderr, "ptr not NULL\n");
        return;
    }

    if (!CRYPTO_THREAD_set_local(&thread_local_key, &destructor_run_count)) {
        fprintf(stderr, "CRYPTO_THREAD_set_local() failed\n");
        return;
    }

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (ptr != &destructor_run_count) {
        fprintf(stderr, "invalid ptr\n");
        return;
    }

    thread_local_thread_cb_ok = 1;
}

static int test_thread_local(void)
{
    thread_t thread;
    void *ptr = NULL;

    if (!CRYPTO_THREAD_init_local(&thread_local_key, thread_local_destructor)) {
        fprintf(stderr, "CRYPTO_THREAD_init_local() failed\n");
        return 0;
    }

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (ptr != NULL) {
        fprintf(stderr, "ptr not NULL\n");
        return 0;
    }

    if (!run_thread(&thread, thread_local_thread_cb) ||
        !wait_for_thread(thread))
    {
        fprintf(stderr, "run_thread() failed\n");
        return 0;
    }

    if (thread_local_thread_cb_ok != 1) {
        fprintf(stderr, "thread-local thread callback failed\n");
        return 0;
    }

#if defined(OPENSSL_THREADS) && !defined(CRYPTO_TDEBUG)

    ptr = CRYPTO_THREAD_get_local(&thread_local_key);
    if (ptr != NULL) {
        fprintf(stderr, "ptr not NULL\n");
        return 0;
    }

# if !defined(OPENSSL_SYS_WINDOWS)
    if (destructor_run_count != 1) {
        fprintf(stderr, "thread-local destructor run %u times\n",
                destructor_run_count);
        return 0;
    }
# endif

#endif

    if (!CRYPTO_THREAD_cleanup_local(&thread_local_key)) {
        fprintf(stderr, "CRYPTO_THREAD_cleanup_local() failed\n");
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    if (!test_lock())
      return 1;

    if (!test_once())
      return 1;

    if (!test_thread_local())
      return 1;

    printf("PASS\n");
    return 0;
}
