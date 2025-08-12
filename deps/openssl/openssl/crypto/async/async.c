/*
 * Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Without this we start getting longjmp crashes because it thinks we're jumping
 * up the stack when in fact we are jumping to an entirely different stack. The
 * cost of this is not having certain buffer overrun/underrun checks etc for
 * this source file :-(
 */
#undef _FORTIFY_SOURCE

/* This must be the first #include file */
#include "async_local.h"

#include <openssl/err.h>
#include "crypto/cryptlib.h"
#include <string.h>

#define ASYNC_JOB_RUNNING   0
#define ASYNC_JOB_PAUSING   1
#define ASYNC_JOB_PAUSED    2
#define ASYNC_JOB_STOPPING  3

static CRYPTO_THREAD_LOCAL ctxkey;
static CRYPTO_THREAD_LOCAL poolkey;

static void async_delete_thread_state(void *arg);

static async_ctx *async_ctx_new(void)
{
    async_ctx *nctx;

    if (!ossl_init_thread_start(NULL, NULL, async_delete_thread_state))
        return NULL;

    nctx = OPENSSL_malloc(sizeof(*nctx));
    if (nctx == NULL)
        goto err;

    async_fibre_init_dispatcher(&nctx->dispatcher);
    nctx->currjob = NULL;
    nctx->blocked = 0;
    if (!CRYPTO_THREAD_set_local(&ctxkey, nctx))
        goto err;

    return nctx;
err:
    OPENSSL_free(nctx);

    return NULL;
}

async_ctx *async_get_ctx(void)
{
    return (async_ctx *)CRYPTO_THREAD_get_local(&ctxkey);
}

static int async_ctx_free(void)
{
    async_ctx *ctx;

    ctx = async_get_ctx();

    if (!CRYPTO_THREAD_set_local(&ctxkey, NULL))
        return 0;

    OPENSSL_free(ctx);

    return 1;
}

static ASYNC_JOB *async_job_new(void)
{
    ASYNC_JOB *job = NULL;

    job = OPENSSL_zalloc(sizeof(*job));
    if (job == NULL)
        return NULL;

    job->status = ASYNC_JOB_RUNNING;

    return job;
}

static void async_job_free(ASYNC_JOB *job)
{
    if (job != NULL) {
        OPENSSL_free(job->funcargs);
        async_fibre_free(&job->fibrectx);
        OPENSSL_free(job);
    }
}

static ASYNC_JOB *async_get_pool_job(void) {
    ASYNC_JOB *job;
    async_pool *pool;

    pool = (async_pool *)CRYPTO_THREAD_get_local(&poolkey);
    if (pool == NULL) {
        /*
         * Pool has not been initialised, so init with the defaults, i.e.
         * no max size and no pre-created jobs
         */
        if (ASYNC_init_thread(0, 0) == 0)
            return NULL;
        pool = (async_pool *)CRYPTO_THREAD_get_local(&poolkey);
    }

    job = sk_ASYNC_JOB_pop(pool->jobs);
    if (job == NULL) {
        /* Pool is empty */
        if ((pool->max_size != 0) && (pool->curr_size >= pool->max_size))
            return NULL;

        job = async_job_new();
        if (job != NULL) {
            if (! async_fibre_makecontext(&job->fibrectx)) {
                async_job_free(job);
                return NULL;
            }
            pool->curr_size++;
        }
    }
    return job;
}

static void async_release_job(ASYNC_JOB *job) {
    async_pool *pool;

    pool = (async_pool *)CRYPTO_THREAD_get_local(&poolkey);
    if (pool == NULL) {
        ERR_raise(ERR_LIB_ASYNC, ERR_R_INTERNAL_ERROR);
        return;
    }
    OPENSSL_free(job->funcargs);
    job->funcargs = NULL;
    sk_ASYNC_JOB_push(pool->jobs, job);
}

void async_start_func(void)
{
    ASYNC_JOB *job;
    async_ctx *ctx = async_get_ctx();

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_ASYNC, ERR_R_INTERNAL_ERROR);
        return;
    }
    while (1) {
        /* Run the job */
        job = ctx->currjob;
        job->ret = job->func(job->funcargs);

        /* Stop the job */
        job->status = ASYNC_JOB_STOPPING;
        if (!async_fibre_swapcontext(&job->fibrectx,
                                     &ctx->dispatcher, 1)) {
            /*
             * Should not happen. Getting here will close the thread...can't do
             * much about it
             */
            ERR_raise(ERR_LIB_ASYNC, ASYNC_R_FAILED_TO_SWAP_CONTEXT);
        }
    }
}

int ASYNC_start_job(ASYNC_JOB **job, ASYNC_WAIT_CTX *wctx, int *ret,
                    int (*func)(void *), void *args, size_t size)
{
    async_ctx *ctx;
    OSSL_LIB_CTX *libctx;

    if (!OPENSSL_init_crypto(OPENSSL_INIT_ASYNC, NULL))
        return ASYNC_ERR;

    ctx = async_get_ctx();
    if (ctx == NULL)
        ctx = async_ctx_new();
    if (ctx == NULL)
        return ASYNC_ERR;

    if (*job != NULL)
        ctx->currjob = *job;

    for (;;) {
        if (ctx->currjob != NULL) {
            if (ctx->currjob->status == ASYNC_JOB_STOPPING) {
                *ret = ctx->currjob->ret;
                ctx->currjob->waitctx = NULL;
                async_release_job(ctx->currjob);
                ctx->currjob = NULL;
                *job = NULL;
                return ASYNC_FINISH;
            }

            if (ctx->currjob->status == ASYNC_JOB_PAUSING) {
                *job = ctx->currjob;
                ctx->currjob->status = ASYNC_JOB_PAUSED;
                ctx->currjob = NULL;
                return ASYNC_PAUSE;
            }

            if (ctx->currjob->status == ASYNC_JOB_PAUSED) {
                if (*job == NULL)
                    return ASYNC_ERR;
                ctx->currjob = *job;

                /*
                 * Restore the default libctx to what it was the last time the
                 * fibre ran
                 */
                libctx = OSSL_LIB_CTX_set0_default(ctx->currjob->libctx);
                if (libctx == NULL) {
                    /* Failed to set the default context */
                    ERR_raise(ERR_LIB_ASYNC, ERR_R_INTERNAL_ERROR);
                    goto err;
                }
                /* Resume previous job */
                if (!async_fibre_swapcontext(&ctx->dispatcher,
                        &ctx->currjob->fibrectx, 1)) {
                    ctx->currjob->libctx = OSSL_LIB_CTX_set0_default(libctx);
                    ERR_raise(ERR_LIB_ASYNC, ASYNC_R_FAILED_TO_SWAP_CONTEXT);
                    goto err;
                }
                /*
                 * In case the fibre changed the default libctx we set it back
                 * again to what it was originally, and remember what it had
                 * been changed to.
                 */
                ctx->currjob->libctx = OSSL_LIB_CTX_set0_default(libctx);
                continue;
            }

            /* Should not happen */
            ERR_raise(ERR_LIB_ASYNC, ERR_R_INTERNAL_ERROR);
            async_release_job(ctx->currjob);
            ctx->currjob = NULL;
            *job = NULL;
            return ASYNC_ERR;
        }

        /* Start a new job */
        if ((ctx->currjob = async_get_pool_job()) == NULL)
            return ASYNC_NO_JOBS;

        if (args != NULL) {
            ctx->currjob->funcargs = OPENSSL_malloc(size);
            if (ctx->currjob->funcargs == NULL) {
                async_release_job(ctx->currjob);
                ctx->currjob = NULL;
                return ASYNC_ERR;
            }
            memcpy(ctx->currjob->funcargs, args, size);
        } else {
            ctx->currjob->funcargs = NULL;
        }

        ctx->currjob->func = func;
        ctx->currjob->waitctx = wctx;
        libctx = ossl_lib_ctx_get_concrete(NULL);
        if (!async_fibre_swapcontext(&ctx->dispatcher,
                &ctx->currjob->fibrectx, 1)) {
            ERR_raise(ERR_LIB_ASYNC, ASYNC_R_FAILED_TO_SWAP_CONTEXT);
            goto err;
        }
        /*
         * In case the fibre changed the default libctx we set it back again
         * to what it was, and remember what it had been changed to.
         */
        ctx->currjob->libctx = OSSL_LIB_CTX_set0_default(libctx);
    }

err:
    async_release_job(ctx->currjob);
    ctx->currjob = NULL;
    *job = NULL;
    return ASYNC_ERR;
}

int ASYNC_pause_job(void)
{
    ASYNC_JOB *job;
    async_ctx *ctx = async_get_ctx();

    if (ctx == NULL
            || ctx->currjob == NULL
            || ctx->blocked) {
        /*
         * Could be we've deliberately not been started within a job so this is
         * counted as success.
         */
        return 1;
    }

    job = ctx->currjob;
    job->status = ASYNC_JOB_PAUSING;

    if (!async_fibre_swapcontext(&job->fibrectx,
                                 &ctx->dispatcher, 1)) {
        ERR_raise(ERR_LIB_ASYNC, ASYNC_R_FAILED_TO_SWAP_CONTEXT);
        return 0;
    }
    /* Reset counts of added and deleted fds */
    async_wait_ctx_reset_counts(job->waitctx);

    return 1;
}

static void async_empty_pool(async_pool *pool)
{
    ASYNC_JOB *job;

    if (pool == NULL || pool->jobs == NULL)
        return;

    do {
        job = sk_ASYNC_JOB_pop(pool->jobs);
        async_job_free(job);
    } while (job);
}

int async_init(void)
{
    if (!CRYPTO_THREAD_init_local(&ctxkey, NULL))
        return 0;

    if (!CRYPTO_THREAD_init_local(&poolkey, NULL)) {
        CRYPTO_THREAD_cleanup_local(&ctxkey);
        return 0;
    }

    return async_local_init();
}

void async_deinit(void)
{
    CRYPTO_THREAD_cleanup_local(&ctxkey);
    CRYPTO_THREAD_cleanup_local(&poolkey);
    async_local_deinit();
}

int ASYNC_init_thread(size_t max_size, size_t init_size)
{
    async_pool *pool;
    size_t curr_size = 0;

    if (init_size > max_size) {
        ERR_raise(ERR_LIB_ASYNC, ASYNC_R_INVALID_POOL_SIZE);
        return 0;
    }

    if (!OPENSSL_init_crypto(OPENSSL_INIT_ASYNC, NULL))
        return 0;

    if (!ossl_init_thread_start(NULL, NULL, async_delete_thread_state))
        return 0;

    pool = OPENSSL_zalloc(sizeof(*pool));
    if (pool == NULL)
        return 0;

    pool->jobs = sk_ASYNC_JOB_new_reserve(NULL, init_size);
    if (pool->jobs == NULL) {
        ERR_raise(ERR_LIB_ASYNC, ERR_R_CRYPTO_LIB);
        OPENSSL_free(pool);
        return 0;
    }

    pool->max_size = max_size;

    /* Pre-create jobs as required */
    while (init_size--) {
        ASYNC_JOB *job;
        job = async_job_new();
        if (job == NULL || !async_fibre_makecontext(&job->fibrectx)) {
            /*
             * Not actually fatal because we already created the pool, just
             * skip creation of any more jobs
             */
            async_job_free(job);
            break;
        }
        job->funcargs = NULL;
        sk_ASYNC_JOB_push(pool->jobs, job); /* Cannot fail due to reserve */
        curr_size++;
    }
    pool->curr_size = curr_size;
    if (!CRYPTO_THREAD_set_local(&poolkey, pool)) {
        ERR_raise(ERR_LIB_ASYNC, ASYNC_R_FAILED_TO_SET_POOL);
        goto err;
    }

    return 1;
err:
    async_empty_pool(pool);
    sk_ASYNC_JOB_free(pool->jobs);
    OPENSSL_free(pool);
    return 0;
}

static void async_delete_thread_state(void *arg)
{
    async_pool *pool = (async_pool *)CRYPTO_THREAD_get_local(&poolkey);

    if (pool != NULL) {
        async_empty_pool(pool);
        sk_ASYNC_JOB_free(pool->jobs);
        OPENSSL_free(pool);
        CRYPTO_THREAD_set_local(&poolkey, NULL);
    }
    async_local_cleanup();
    async_ctx_free();
}

void ASYNC_cleanup_thread(void)
{
    if (!OPENSSL_init_crypto(OPENSSL_INIT_ASYNC, NULL))
        return;

    async_delete_thread_state(NULL);
}

ASYNC_JOB *ASYNC_get_current_job(void)
{
    async_ctx *ctx;

    if (!OPENSSL_init_crypto(OPENSSL_INIT_ASYNC, NULL))
        return NULL;

    ctx = async_get_ctx();
    if (ctx == NULL)
        return NULL;

    return ctx->currjob;
}

ASYNC_WAIT_CTX *ASYNC_get_wait_ctx(ASYNC_JOB *job)
{
    return job->waitctx;
}

void ASYNC_block_pause(void)
{
    async_ctx *ctx;

    if (!OPENSSL_init_crypto(OPENSSL_INIT_ASYNC, NULL))
        return;

    ctx = async_get_ctx();
    if (ctx == NULL || ctx->currjob == NULL) {
        /*
         * We're not in a job anyway so ignore this
         */
        return;
    }
    ctx->blocked++;
}

void ASYNC_unblock_pause(void)
{
    async_ctx *ctx;

    if (!OPENSSL_init_crypto(OPENSSL_INIT_ASYNC, NULL))
        return;

    ctx = async_get_ctx();
    if (ctx == NULL || ctx->currjob == NULL) {
        /*
         * We're not in a job anyway so ignore this
         */
        return;
    }
    if (ctx->blocked > 0)
        ctx->blocked--;
}
