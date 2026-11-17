/*
 * Copyright 2015-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Must do this before including any header files, because on MacOS/X <stlib.h>
 * includes <signal.h> which includes <ucontext.h>
 */
#if !defined(OSSL_LIBCRYPTO_ASYNC_ASYNC_LOCAL_H)
#define OSSL_LIBCRYPTO_ASYNC_ASYNC_LOCAL_H

#if defined(__APPLE__) && defined(__MACH__) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE /* Otherwise incomplete ucontext_t structure */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <openssl/crypto.h>
#include <openssl/e_os2.h>
#include <internal/e_os.h>

typedef struct async_ctx_st async_ctx;
typedef struct async_pool_st async_pool;

#if defined(_WIN32)
#define ASYNC_WIN
#define ASYNC_ARCH

#include "internal/cryptlib.h"

typedef struct async_fibre_st {
    LPVOID fibre;
    int converted;
} async_fibre;

#define async_fibre_swapcontext(o, n, r) \
    (SwitchToFiber((n)->fibre), 1)

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x600
#define async_fibre_makecontext(c)                             \
    ((c)->fibre = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, \
         async_start_func_win, 0))
#else
#define async_fibre_makecontext(c) \
    ((c)->fibre = CreateFiber(0, async_start_func_win, 0))
#endif

#define async_fibre_free(f) (DeleteFiber((f)->fibre))
#define async_local_init() 1
#define async_local_deinit()

int async_fibre_init_dispatcher(async_fibre *fibre);
VOID CALLBACK async_start_func_win(PVOID unused);

#elif defined(OPENSSL_SYS_UNIX)                               \
    && defined(OPENSSL_THREADS) && !defined(OPENSSL_NO_ASYNC) \
    && !defined(__ANDROID__) && !defined(__OpenBSD__)         \
    && !defined(OPENSSL_SYS_TANDEM)

#include <unistd.h>

#if _POSIX_VERSION >= 200112L \
    && (_POSIX_VERSION < 200809L || defined(__GLIBC__) || defined(__FreeBSD__))

#include <pthread.h>

#define ASYNC_POSIX
#define ASYNC_ARCH

#if defined(__CET__) || defined(__ia64__)
/*
 * When Intel CET is enabled, makecontext will create a different
 * shadow stack for each context.  async_fibre_swapcontext cannot
 * use _longjmp.  It must call swapcontext to swap shadow stack as
 * well as normal stack.
 * On IA64 the register stack engine is not saved across setjmp/longjmp. Here
 * swapcontext() performs correctly.
 */
#define USE_SWAPCONTEXT
#endif
#if defined(__aarch64__) && defined(__clang__) \
    && defined(__ARM_FEATURE_BTI_DEFAULT) && __ARM_FEATURE_BTI_DEFAULT == 1
/*
 * setjmp/longjmp don't currently work with BTI on all libc implementations
 * when compiled by clang. This is because clang doesn't put a BTI after the
 * call to setjmp where it returns the second time. This then fails on libc
 * implementations - notably glibc - which use an indirect jump to there.
 * So use the swapcontext implementation, which does work.
 * See https://github.com/llvm/llvm-project/issues/48888.
 */
#define USE_SWAPCONTEXT
#endif
#include <ucontext.h>
#ifndef USE_SWAPCONTEXT
#include <setjmp.h>
#endif

typedef struct async_fibre_st {
    ucontext_t fibre;
#ifndef USE_SWAPCONTEXT
    jmp_buf env;
    int env_init;
#endif
} async_fibre;

int async_local_init(void);
void async_local_deinit(void);

static ossl_inline int async_fibre_swapcontext(async_fibre *o, async_fibre *n, int r)
{
#ifdef USE_SWAPCONTEXT
    swapcontext(&o->fibre, &n->fibre);
#else
    o->env_init = 1;

    if (!r || !_setjmp(o->env)) {
        if (n->env_init)
            _longjmp(n->env, 1);
        else
            setcontext(&n->fibre);
    }
#endif

    return 1;
}

#define async_fibre_init_dispatcher(d)

int async_fibre_makecontext(async_fibre *fibre);
void async_fibre_free(async_fibre *fibre);

#endif
#endif /* UNIX */

#ifndef ASYNC_ARCH
#define ASYNC_NULL
#define ASYNC_ARCH

typedef struct async_fibre_st {
    int dummy;
} async_fibre;

#define async_fibre_swapcontext(o, n, r) 0
#define async_fibre_makecontext(c) 0
#define async_fibre_free(f)
#define async_fibre_init_dispatcher(f)
#define async_local_init() 1
#define async_local_deinit()
#endif

/* needs to be included after windows.h */
#include <openssl/async.h>
#include "crypto/async.h"

struct async_ctx_st {
    async_fibre dispatcher;
    ASYNC_JOB *currjob;
    unsigned int blocked;
};

struct async_job_st {
    async_fibre fibrectx;
    int (*func)(void *);
    void *funcargs;
    int ret;
    int status;
    ASYNC_WAIT_CTX *waitctx;
    OSSL_LIB_CTX *libctx;
};

struct fd_lookup_st {
    const void *key;
    OSSL_ASYNC_FD fd;
    void *custom_data;
    void (*cleanup)(ASYNC_WAIT_CTX *, const void *, OSSL_ASYNC_FD, void *);
    int add;
    int del;
    struct fd_lookup_st *next;
};

struct async_wait_ctx_st {
    struct fd_lookup_st *fds;
    size_t numadd;
    size_t numdel;
    ASYNC_callback_fn callback;
    void *callback_arg;
    int status;
};

DEFINE_STACK_OF(ASYNC_JOB)

struct async_pool_st {
    STACK_OF(ASYNC_JOB) *jobs;
    size_t curr_size;
    size_t max_size;
};

void async_local_cleanup(void);
void async_start_func(void);
async_ctx *async_get_ctx(void);

void async_wait_ctx_reset_counts(ASYNC_WAIT_CTX *ctx);

#endif /* !defined(OSSL_LIBCRYPTO_ASYNC_ASYNC_LOCAL_H) */
