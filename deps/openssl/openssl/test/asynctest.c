/*
 * Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifdef _WIN32
# include <windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <openssl/async.h>
#include <openssl/crypto.h>

static int ctr = 0;
static ASYNC_JOB *currjob = NULL;

static int only_pause(void *args)
{
    ASYNC_pause_job();

    return 1;
}

static int add_two(void *args)
{
    ctr++;
    ASYNC_pause_job();
    ctr++;

    return 2;
}

static int save_current(void *args)
{
    currjob = ASYNC_get_current_job();
    ASYNC_pause_job();

    return 1;
}

#define MAGIC_WAIT_FD   ((OSSL_ASYNC_FD)99)
static int waitfd(void *args)
{
    ASYNC_JOB *job;
    ASYNC_WAIT_CTX *waitctx;
    job = ASYNC_get_current_job();
    if (job == NULL)
        return 0;
    waitctx = ASYNC_get_wait_ctx(job);
    if (waitctx == NULL)
        return 0;

    /* First case: no fd added or removed */
    ASYNC_pause_job();

    /* Second case: one fd added */
    if (!ASYNC_WAIT_CTX_set_wait_fd(waitctx, waitctx, MAGIC_WAIT_FD, NULL, NULL))
        return 0;
    ASYNC_pause_job();

    /* Third case: all fd removed */
    if (!ASYNC_WAIT_CTX_clear_fd(waitctx, waitctx))
        return 0;
    ASYNC_pause_job();

    /* Last case: fd added and immediately removed */
    if (!ASYNC_WAIT_CTX_set_wait_fd(waitctx, waitctx, MAGIC_WAIT_FD, NULL, NULL))
        return 0;
    if (!ASYNC_WAIT_CTX_clear_fd(waitctx, waitctx))
        return 0;

    return 1;
}

static int blockpause(void *args)
{
    ASYNC_block_pause();
    ASYNC_pause_job();
    ASYNC_unblock_pause();
    ASYNC_pause_job();

    return 1;
}

static int test_ASYNC_init_thread(void)
{
    ASYNC_JOB *job1 = NULL, *job2 = NULL, *job3 = NULL;
    int funcret1, funcret2, funcret3;
    ASYNC_WAIT_CTX *waitctx = NULL;

    if (       !ASYNC_init_thread(2, 0)
            || (waitctx = ASYNC_WAIT_CTX_new()) == NULL
            || ASYNC_start_job(&job1, waitctx, &funcret1, only_pause, NULL, 0)
                != ASYNC_PAUSE
            || ASYNC_start_job(&job2, waitctx, &funcret2, only_pause, NULL, 0)
                != ASYNC_PAUSE
            || ASYNC_start_job(&job3, waitctx, &funcret3, only_pause, NULL, 0)
                != ASYNC_NO_JOBS
            || ASYNC_start_job(&job1, waitctx, &funcret1, only_pause, NULL, 0)
                != ASYNC_FINISH
            || ASYNC_start_job(&job3, waitctx, &funcret3, only_pause, NULL, 0)
                != ASYNC_PAUSE
            || ASYNC_start_job(&job2, waitctx, &funcret2, only_pause, NULL, 0)
                != ASYNC_FINISH
            || ASYNC_start_job(&job3, waitctx, &funcret3, only_pause, NULL, 0)
                != ASYNC_FINISH
            || funcret1 != 1
            || funcret2 != 1
            || funcret3 != 1) {
        fprintf(stderr, "test_ASYNC_init_thread() failed\n");
        ASYNC_WAIT_CTX_free(waitctx);
        ASYNC_cleanup_thread();
        return 0;
    }

    ASYNC_WAIT_CTX_free(waitctx);
    ASYNC_cleanup_thread();
    return 1;
}

static int test_ASYNC_start_job(void)
{
    ASYNC_JOB *job = NULL;
    int funcret;
    ASYNC_WAIT_CTX *waitctx = NULL;

    ctr = 0;

    if (       !ASYNC_init_thread(1, 0)
            || (waitctx = ASYNC_WAIT_CTX_new()) == NULL
            || ASYNC_start_job(&job, waitctx, &funcret, add_two, NULL, 0)
               != ASYNC_PAUSE
            || ctr != 1
            || ASYNC_start_job(&job, waitctx, &funcret, add_two, NULL, 0)
               != ASYNC_FINISH
            || ctr != 2
            || funcret != 2) {
        fprintf(stderr, "test_ASYNC_start_job() failed\n");
        ASYNC_WAIT_CTX_free(waitctx);
        ASYNC_cleanup_thread();
        return 0;
    }

    ASYNC_WAIT_CTX_free(waitctx);
    ASYNC_cleanup_thread();
    return 1;
}

static int test_ASYNC_get_current_job(void)
{
    ASYNC_JOB *job = NULL;
    int funcret;
    ASYNC_WAIT_CTX *waitctx = NULL;

    currjob = NULL;

    if (       !ASYNC_init_thread(1, 0)
            || (waitctx = ASYNC_WAIT_CTX_new()) == NULL
            || ASYNC_start_job(&job, waitctx, &funcret, save_current, NULL, 0)
                != ASYNC_PAUSE
            || currjob != job
            || ASYNC_start_job(&job, waitctx, &funcret, save_current, NULL, 0)
                != ASYNC_FINISH
            || funcret != 1) {
        fprintf(stderr, "test_ASYNC_get_current_job() failed\n");
        ASYNC_WAIT_CTX_free(waitctx);
        ASYNC_cleanup_thread();
        return 0;
    }

    ASYNC_WAIT_CTX_free(waitctx);
    ASYNC_cleanup_thread();
    return 1;
}

static int test_ASYNC_WAIT_CTX_get_all_fds(void)
{
    ASYNC_JOB *job = NULL;
    int funcret;
    ASYNC_WAIT_CTX *waitctx = NULL;
    OSSL_ASYNC_FD fd = OSSL_BAD_ASYNC_FD, delfd = OSSL_BAD_ASYNC_FD;
    size_t numfds, numdelfds;

    if (       !ASYNC_init_thread(1, 0)
            || (waitctx = ASYNC_WAIT_CTX_new()) == NULL
               /* On first run we're not expecting any wait fds */
            || ASYNC_start_job(&job, waitctx, &funcret, waitfd, NULL, 0)
                != ASYNC_PAUSE
            || !ASYNC_WAIT_CTX_get_all_fds(waitctx, NULL, &numfds)
            || numfds != 0
            || !ASYNC_WAIT_CTX_get_changed_fds(waitctx, NULL, &numfds, NULL,
                                               &numdelfds)
            || numfds != 0
            || numdelfds != 0
               /* On second run we're expecting one added fd */
            || ASYNC_start_job(&job, waitctx, &funcret, waitfd, NULL, 0)
                != ASYNC_PAUSE
            || !ASYNC_WAIT_CTX_get_all_fds(waitctx, NULL, &numfds)
            || numfds != 1
            || !ASYNC_WAIT_CTX_get_all_fds(waitctx, &fd, &numfds)
            || fd != MAGIC_WAIT_FD
            || (fd = OSSL_BAD_ASYNC_FD, 0) /* Assign to something else */
            || !ASYNC_WAIT_CTX_get_changed_fds(waitctx, NULL, &numfds, NULL,
                                               &numdelfds)
            || numfds != 1
            || numdelfds != 0
            || !ASYNC_WAIT_CTX_get_changed_fds(waitctx, &fd, &numfds, NULL,
                                               &numdelfds)
            || fd != MAGIC_WAIT_FD
               /* On third run we expect one deleted fd */
            || ASYNC_start_job(&job, waitctx, &funcret, waitfd, NULL, 0)
                != ASYNC_PAUSE
            || !ASYNC_WAIT_CTX_get_all_fds(waitctx, NULL, &numfds)
            || numfds != 0
            || !ASYNC_WAIT_CTX_get_changed_fds(waitctx, NULL, &numfds, NULL,
                                               &numdelfds)
            || numfds != 0
            || numdelfds != 1
            || !ASYNC_WAIT_CTX_get_changed_fds(waitctx, NULL, &numfds, &delfd,
                                               &numdelfds)
            || delfd != MAGIC_WAIT_FD
            /* On last run we are not expecting any wait fd */
            || ASYNC_start_job(&job, waitctx, &funcret, waitfd, NULL, 0)
                != ASYNC_FINISH
            || !ASYNC_WAIT_CTX_get_all_fds(waitctx, NULL, &numfds)
            || numfds != 0
            || !ASYNC_WAIT_CTX_get_changed_fds(waitctx, NULL, &numfds, NULL,
                                               &numdelfds)
            || numfds != 0
            || numdelfds != 0
            || funcret != 1) {
        fprintf(stderr, "test_ASYNC_get_wait_fd() failed\n");
        ASYNC_WAIT_CTX_free(waitctx);
        ASYNC_cleanup_thread();
        return 0;
    }

    ASYNC_WAIT_CTX_free(waitctx);
    ASYNC_cleanup_thread();
    return 1;
}

static int test_ASYNC_block_pause(void)
{
    ASYNC_JOB *job = NULL;
    int funcret;
    ASYNC_WAIT_CTX *waitctx = NULL;

    if (       !ASYNC_init_thread(1, 0)
            || (waitctx = ASYNC_WAIT_CTX_new()) == NULL
            || ASYNC_start_job(&job, waitctx, &funcret, blockpause, NULL, 0)
                != ASYNC_PAUSE
            || ASYNC_start_job(&job, waitctx, &funcret, blockpause, NULL, 0)
                != ASYNC_FINISH
            || funcret != 1) {
        fprintf(stderr, "test_ASYNC_block_pause() failed\n");
        ASYNC_WAIT_CTX_free(waitctx);
        ASYNC_cleanup_thread();
        return 0;
    }

    ASYNC_WAIT_CTX_free(waitctx);
    ASYNC_cleanup_thread();
    return 1;
}

int main(int argc, char **argv)
{
    if (!ASYNC_is_capable()) {
        fprintf(stderr,
                "OpenSSL build is not ASYNC capable - skipping async tests\n");
    } else {
        CRYPTO_set_mem_debug(1);
        CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

        if (       !test_ASYNC_init_thread()
                || !test_ASYNC_start_job()
                || !test_ASYNC_get_current_job()
                || !test_ASYNC_WAIT_CTX_get_all_fds()
                || !test_ASYNC_block_pause()) {
            return 1;
        }
    }
    printf("PASS\n");
    return 0;
}
