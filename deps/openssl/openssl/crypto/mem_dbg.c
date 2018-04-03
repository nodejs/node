/* crypto/mem_dbg.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2018 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "cryptlib.h"
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/lhash.h>

static int mh_mode = CRYPTO_MEM_CHECK_OFF;
/*
 * The state changes to CRYPTO_MEM_CHECK_ON | CRYPTO_MEM_CHECK_ENABLE when
 * the application asks for it (usually after library initialisation for
 * which no book-keeping is desired). State CRYPTO_MEM_CHECK_ON exists only
 * temporarily when the library thinks that certain allocations should not be
 * checked (e.g. the data structures used for memory checking).  It is not
 * suitable as an initial state: the library will unexpectedly enable memory
 * checking when it executes one of those sections that want to disable
 * checking temporarily. State CRYPTO_MEM_CHECK_ENABLE without ..._ON makes
 * no sense whatsoever.
 */

static unsigned long order = 0; /* number of memory requests */

DECLARE_LHASH_OF(MEM);
static LHASH_OF(MEM) *mh = NULL; /* hash-table of memory requests (address as
                                  * key); access requires MALLOC2 lock */

typedef struct app_mem_info_st
/*-
 * For application-defined information (static C-string `info')
 * to be displayed in memory leak list.
 * Each thread has its own stack.  For applications, there is
 *   CRYPTO_push_info("...")     to push an entry,
 *   CRYPTO_pop_info()           to pop an entry,
 *   CRYPTO_remove_all_info()    to pop all entries.
 */
{
    CRYPTO_THREADID threadid;
    const char *file;
    int line;
    const char *info;
    struct app_mem_info_st *next; /* tail of thread's stack */
    int references;
} APP_INFO;

static void app_info_free(APP_INFO *);

DECLARE_LHASH_OF(APP_INFO);
static LHASH_OF(APP_INFO) *amih = NULL; /* hash-table with those
                                         * app_mem_info_st's that are at the
                                         * top of their thread's stack (with
                                         * `thread' as key); access requires
                                         * MALLOC2 lock */

typedef struct mem_st
/* memory-block description */
{
    void *addr;
    int num;
    const char *file;
    int line;
    CRYPTO_THREADID threadid;
    unsigned long order;
    time_t time;
    APP_INFO *app_info;
} MEM;

static long options =           /* extra information to be recorded */
#if defined(CRYPTO_MDEBUG_TIME) || defined(CRYPTO_MDEBUG_ALL)
    V_CRYPTO_MDEBUG_TIME |
#endif
#if defined(CRYPTO_MDEBUG_THREAD) || defined(CRYPTO_MDEBUG_ALL)
    V_CRYPTO_MDEBUG_THREAD |
#endif
    0;

static unsigned int num_disable = 0; /* num_disable > 0 iff mh_mode ==
                                      * CRYPTO_MEM_CHECK_ON (w/o ..._ENABLE) */

/*
 * Valid iff num_disable > 0.  CRYPTO_LOCK_MALLOC2 is locked exactly in this
 * case (by the thread named in disabling_thread).
 */
static CRYPTO_THREADID disabling_threadid;

static void app_info_free(APP_INFO *inf)
{
    if (--(inf->references) <= 0) {
        if (inf->next != NULL) {
            app_info_free(inf->next);
        }
        OPENSSL_free(inf);
    }
}

int CRYPTO_mem_ctrl(int mode)
{
    int ret = mh_mode;

    CRYPTO_w_lock(CRYPTO_LOCK_MALLOC);
    switch (mode) {
        /*
         * for applications (not to be called while multiple threads use the
         * library):
         */
    case CRYPTO_MEM_CHECK_ON:  /* aka MemCheck_start() */
        mh_mode = CRYPTO_MEM_CHECK_ON | CRYPTO_MEM_CHECK_ENABLE;
        num_disable = 0;
        break;
    case CRYPTO_MEM_CHECK_OFF: /* aka MemCheck_stop() */
        mh_mode = 0;
        num_disable = 0;        /* should be true *before* MemCheck_stop is
                                 * used, or there'll be a lot of confusion */
        break;

        /* switch off temporarily (for library-internal use): */
    case CRYPTO_MEM_CHECK_DISABLE: /* aka MemCheck_off() */
        if (mh_mode & CRYPTO_MEM_CHECK_ON) {
            CRYPTO_THREADID cur;
            CRYPTO_THREADID_current(&cur);
            /* see if we don't have the MALLOC2 lock already */
            if (!num_disable
                || CRYPTO_THREADID_cmp(&disabling_threadid, &cur)) {
                /*
                 * Long-time lock CRYPTO_LOCK_MALLOC2 must not be claimed
                 * while we're holding CRYPTO_LOCK_MALLOC, or we'll deadlock
                 * if somebody else holds CRYPTO_LOCK_MALLOC2 (and cannot
                 * release it because we block entry to this function). Give
                 * them a chance, first, and then claim the locks in
                 * appropriate order (long-time lock first).
                 */
                CRYPTO_w_unlock(CRYPTO_LOCK_MALLOC);
                /*
                 * Note that after we have waited for CRYPTO_LOCK_MALLOC2 and
                 * CRYPTO_LOCK_MALLOC, we'll still be in the right "case" and
                 * "if" branch because MemCheck_start and MemCheck_stop may
                 * never be used while there are multiple OpenSSL threads.
                 */
                CRYPTO_w_lock(CRYPTO_LOCK_MALLOC2);
                CRYPTO_w_lock(CRYPTO_LOCK_MALLOC);
                mh_mode &= ~CRYPTO_MEM_CHECK_ENABLE;
                CRYPTO_THREADID_cpy(&disabling_threadid, &cur);
            }
            num_disable++;
        }
        break;
    case CRYPTO_MEM_CHECK_ENABLE: /* aka MemCheck_on() */
        if (mh_mode & CRYPTO_MEM_CHECK_ON) {
            if (num_disable) {  /* always true, or something is going wrong */
                num_disable--;
                if (num_disable == 0) {
                    mh_mode |= CRYPTO_MEM_CHECK_ENABLE;
                    CRYPTO_w_unlock(CRYPTO_LOCK_MALLOC2);
                }
            }
        }
        break;

    default:
        break;
    }
    CRYPTO_w_unlock(CRYPTO_LOCK_MALLOC);
    return (ret);
}

int CRYPTO_is_mem_check_on(void)
{
    int ret = 0;

    if (mh_mode & CRYPTO_MEM_CHECK_ON) {
        CRYPTO_THREADID cur;
        CRYPTO_THREADID_current(&cur);
        CRYPTO_r_lock(CRYPTO_LOCK_MALLOC);

        ret = (mh_mode & CRYPTO_MEM_CHECK_ENABLE)
            || CRYPTO_THREADID_cmp(&disabling_threadid, &cur);

        CRYPTO_r_unlock(CRYPTO_LOCK_MALLOC);
    }
    return (ret);
}

void CRYPTO_dbg_set_options(long bits)
{
    options = bits;
}

long CRYPTO_dbg_get_options(void)
{
    return options;
}

static int mem_cmp(const MEM *a, const MEM *b)
{
#ifdef _WIN64
    const char *ap = (const char *)a->addr, *bp = (const char *)b->addr;
    if (ap == bp)
        return 0;
    else if (ap > bp)
        return 1;
    else
        return -1;
#else
    return (const char *)a->addr - (const char *)b->addr;
#endif
}

static IMPLEMENT_LHASH_COMP_FN(mem, MEM)

static unsigned long mem_hash(const MEM *a)
{
    unsigned long ret;

    ret = (unsigned long)a->addr;

    ret = ret * 17851 + (ret >> 14) * 7 + (ret >> 4) * 251;
    return (ret);
}

static IMPLEMENT_LHASH_HASH_FN(mem, MEM)

/* static int app_info_cmp(APP_INFO *a, APP_INFO *b) */
static int app_info_cmp(const void *a_void, const void *b_void)
{
    return CRYPTO_THREADID_cmp(&((const APP_INFO *)a_void)->threadid,
                               &((const APP_INFO *)b_void)->threadid);
}

static IMPLEMENT_LHASH_COMP_FN(app_info, APP_INFO)

static unsigned long app_info_hash(const APP_INFO *a)
{
    unsigned long ret;

    ret = CRYPTO_THREADID_hash(&a->threadid);
    /* This is left in as a "who am I to question legacy?" measure */
    ret = ret * 17851 + (ret >> 14) * 7 + (ret >> 4) * 251;
    return (ret);
}

static IMPLEMENT_LHASH_HASH_FN(app_info, APP_INFO)

static APP_INFO *pop_info(void)
{
    APP_INFO tmp;
    APP_INFO *ret = NULL;

    if (amih != NULL) {
        CRYPTO_THREADID_current(&tmp.threadid);
        if ((ret = lh_APP_INFO_delete(amih, &tmp)) != NULL) {
            APP_INFO *next = ret->next;

            if (next != NULL) {
                next->references++;
                (void)lh_APP_INFO_insert(amih, next);
            }
#ifdef LEVITTE_DEBUG_MEM
            if (CRYPTO_THREADID_cmp(&ret->threadid, &tmp.threadid)) {
                fprintf(stderr,
                        "pop_info(): deleted info has other thread ID (%lu) than the current thread (%lu)!!!!\n",
                        CRYPTO_THREADID_hash(&ret->threadid),
                        CRYPTO_THREADID_hash(&tmp.threadid));
                abort();
            }
#endif
            if (--(ret->references) <= 0) {
                ret->next = NULL;
                if (next != NULL)
                    next->references--;
                OPENSSL_free(ret);
            }
        }
    }
    return (ret);
}

int CRYPTO_push_info_(const char *info, const char *file, int line)
{
    APP_INFO *ami, *amim;
    int ret = 0;

    if (is_MemCheck_on()) {
        MemCheck_off();         /* obtain MALLOC2 lock */

        if ((ami = (APP_INFO *)OPENSSL_malloc(sizeof(APP_INFO))) == NULL) {
            ret = 0;
            goto err;
        }
        if (amih == NULL) {
            if ((amih = lh_APP_INFO_new()) == NULL) {
                OPENSSL_free(ami);
                ret = 0;
                goto err;
            }
        }

        CRYPTO_THREADID_current(&ami->threadid);
        ami->file = file;
        ami->line = line;
        ami->info = info;
        ami->references = 1;
        ami->next = NULL;

        if ((amim = lh_APP_INFO_insert(amih, ami)) != NULL) {
#ifdef LEVITTE_DEBUG_MEM
            if (CRYPTO_THREADID_cmp(&ami->threadid, &amim->threadid)) {
                fprintf(stderr,
                        "CRYPTO_push_info(): previous info has other thread ID (%lu) than the current thread (%lu)!!!!\n",
                        CRYPTO_THREADID_hash(&amim->threadid),
                        CRYPTO_THREADID_hash(&ami->threadid));
                abort();
            }
#endif
            ami->next = amim;
        }
 err:
        MemCheck_on();          /* release MALLOC2 lock */
    }

    return (ret);
}

int CRYPTO_pop_info(void)
{
    int ret = 0;

    if (is_MemCheck_on()) {     /* _must_ be true, or something went severely
                                 * wrong */
        MemCheck_off();         /* obtain MALLOC2 lock */

        ret = (pop_info() != NULL);

        MemCheck_on();          /* release MALLOC2 lock */
    }
    return (ret);
}

int CRYPTO_remove_all_info(void)
{
    int ret = 0;

    if (is_MemCheck_on()) {     /* _must_ be true */
        MemCheck_off();         /* obtain MALLOC2 lock */

        while (pop_info() != NULL)
            ret++;

        MemCheck_on();          /* release MALLOC2 lock */
    }
    return (ret);
}

static unsigned long break_order_num = 0;
void CRYPTO_dbg_malloc(void *addr, int num, const char *file, int line,
                       int before_p)
{
    MEM *m, *mm;
    APP_INFO tmp, *amim;

    switch (before_p & 127) {
    case 0:
        break;
    case 1:
        if (addr == NULL)
            break;

        if (is_MemCheck_on()) {
            MemCheck_off();     /* make sure we hold MALLOC2 lock */
            if ((m = (MEM *)OPENSSL_malloc(sizeof(MEM))) == NULL) {
                OPENSSL_free(addr);
                MemCheck_on();  /* release MALLOC2 lock if num_disabled drops
                                 * to 0 */
                return;
            }
            if (mh == NULL) {
                if ((mh = lh_MEM_new()) == NULL) {
                    OPENSSL_free(addr);
                    OPENSSL_free(m);
                    addr = NULL;
                    goto err;
                }
            }

            m->addr = addr;
            m->file = file;
            m->line = line;
            m->num = num;
            if (options & V_CRYPTO_MDEBUG_THREAD)
                CRYPTO_THREADID_current(&m->threadid);
            else
                memset(&m->threadid, 0, sizeof(m->threadid));

            if (order == break_order_num) {
                /* BREAK HERE */
                m->order = order;
            }
            m->order = order++;
#ifdef LEVITTE_DEBUG_MEM
            fprintf(stderr, "LEVITTE_DEBUG_MEM: [%5ld] %c 0x%p (%d)\n",
                    m->order, (before_p & 128) ? '*' : '+', m->addr, m->num);
#endif
            if (options & V_CRYPTO_MDEBUG_TIME)
                m->time = time(NULL);
            else
                m->time = 0;

            CRYPTO_THREADID_current(&tmp.threadid);
            m->app_info = NULL;
            if (amih != NULL
                && (amim = lh_APP_INFO_retrieve(amih, &tmp)) != NULL) {
                m->app_info = amim;
                amim->references++;
            }

            if ((mm = lh_MEM_insert(mh, m)) != NULL) {
                /* Not good, but don't sweat it */
                if (mm->app_info != NULL) {
                    mm->app_info->references--;
                }
                OPENSSL_free(mm);
            }
 err:
            MemCheck_on();      /* release MALLOC2 lock if num_disabled drops
                                 * to 0 */
        }
        break;
    }
    return;
}

void CRYPTO_dbg_free(void *addr, int before_p)
{
    MEM m, *mp;

    switch (before_p) {
    case 0:
        if (addr == NULL)
            break;

        if (is_MemCheck_on() && (mh != NULL)) {
            MemCheck_off();     /* make sure we hold MALLOC2 lock */

            m.addr = addr;
            mp = lh_MEM_delete(mh, &m);
            if (mp != NULL) {
#ifdef LEVITTE_DEBUG_MEM
                fprintf(stderr, "LEVITTE_DEBUG_MEM: [%5ld] - 0x%p (%d)\n",
                        mp->order, mp->addr, mp->num);
#endif
                if (mp->app_info != NULL)
                    app_info_free(mp->app_info);
                OPENSSL_free(mp);
            }

            MemCheck_on();      /* release MALLOC2 lock if num_disabled drops
                                 * to 0 */
        }
        break;
    case 1:
        break;
    }
}

void CRYPTO_dbg_realloc(void *addr1, void *addr2, int num,
                        const char *file, int line, int before_p)
{
    MEM m, *mp;

#ifdef LEVITTE_DEBUG_MEM
    fprintf(stderr,
            "LEVITTE_DEBUG_MEM: --> CRYPTO_dbg_malloc(addr1 = %p, addr2 = %p, num = %d, file = \"%s\", line = %d, before_p = %d)\n",
            addr1, addr2, num, file, line, before_p);
#endif

    switch (before_p) {
    case 0:
        break;
    case 1:
        if (addr2 == NULL)
            break;

        if (addr1 == NULL) {
            CRYPTO_dbg_malloc(addr2, num, file, line, 128 | before_p);
            break;
        }

        if (is_MemCheck_on()) {
            MemCheck_off();     /* make sure we hold MALLOC2 lock */

            m.addr = addr1;
            mp = lh_MEM_delete(mh, &m);
            if (mp != NULL) {
#ifdef LEVITTE_DEBUG_MEM
                fprintf(stderr,
                        "LEVITTE_DEBUG_MEM: [%5ld] * 0x%p (%d) -> 0x%p (%d)\n",
                        mp->order, mp->addr, mp->num, addr2, num);
#endif
                mp->addr = addr2;
                mp->num = num;
                (void)lh_MEM_insert(mh, mp);
            }

            MemCheck_on();      /* release MALLOC2 lock if num_disabled drops
                                 * to 0 */
        }
        break;
    }
    return;
}

typedef struct mem_leak_st {
    BIO *bio;
    int chunks;
    long bytes;
} MEM_LEAK;

static void print_leak_doall_arg(const MEM *m, MEM_LEAK *l)
{
    char buf[1024];
    char *bufp = buf;
    APP_INFO *amip;
    int ami_cnt;
    struct tm *lcl = NULL;
    struct tm result = {0};
    CRYPTO_THREADID ti;

#define BUF_REMAIN (sizeof(buf) - (size_t)(bufp - buf))

    if (m->addr == (char *)l->bio)
        return;

    if (options & V_CRYPTO_MDEBUG_TIME) {
# if defined(OPENSSL_THREADS) && !defined(OPENSSL_SYS_WIN32) && \
            !defined(OPENSSL_SYS_OS2) && !defined(OPENSSL_SYS_SUNOS) && \
            (!defined(OPENSSL_SYS_VMS) || defined(localtime_r))
        lcl = localtime_r(&m->time, &result);
# else
        lcl = localtime(&m->time);
# endif
        BIO_snprintf(bufp, BUF_REMAIN, "[%02d:%02d:%02d] ",
                     lcl->tm_hour, lcl->tm_min, lcl->tm_sec);
        bufp += strlen(bufp);
    }

    BIO_snprintf(bufp, BUF_REMAIN, "%5lu file=%s, line=%d, ",
                 m->order, m->file, m->line);
    bufp += strlen(bufp);

    if (options & V_CRYPTO_MDEBUG_THREAD) {
        BIO_snprintf(bufp, BUF_REMAIN, "thread=%lu, ",
                     CRYPTO_THREADID_hash(&m->threadid));
        bufp += strlen(bufp);
    }

    BIO_snprintf(bufp, BUF_REMAIN, "number=%d, address=%08lX\n",
                 m->num, (unsigned long)m->addr);
    bufp += strlen(bufp);

    BIO_puts(l->bio, buf);

    l->chunks++;
    l->bytes += m->num;

    amip = m->app_info;
    ami_cnt = 0;
    if (!amip)
        return;
    CRYPTO_THREADID_cpy(&ti, &amip->threadid);

    do {
        int buf_len;
        int info_len;

        ami_cnt++;
        memset(buf, '>', ami_cnt);
        BIO_snprintf(buf + ami_cnt, sizeof(buf) - ami_cnt,
                     " thread=%lu, file=%s, line=%d, info=\"",
                     CRYPTO_THREADID_hash(&amip->threadid), amip->file,
                     amip->line);
        buf_len = strlen(buf);
        info_len = strlen(amip->info);
        if (128 - buf_len - 3 < info_len) {
            memcpy(buf + buf_len, amip->info, 128 - buf_len - 3);
            buf_len = 128 - 3;
        } else {
            BUF_strlcpy(buf + buf_len, amip->info, sizeof(buf) - buf_len);
            buf_len = strlen(buf);
        }
        BIO_snprintf(buf + buf_len, sizeof(buf) - buf_len, "\"\n");

        BIO_puts(l->bio, buf);

        amip = amip->next;
    }
    while (amip && !CRYPTO_THREADID_cmp(&amip->threadid, &ti));

#ifdef LEVITTE_DEBUG_MEM
    if (amip) {
        fprintf(stderr, "Thread switch detected in backtrace!!!!\n");
        abort();
    }
#endif
}

static IMPLEMENT_LHASH_DOALL_ARG_FN(print_leak, const MEM, MEM_LEAK)

void CRYPTO_mem_leaks(BIO *b)
{
    MEM_LEAK ml;

    if (mh == NULL && amih == NULL)
        return;

    MemCheck_off();             /* obtain MALLOC2 lock */

    ml.bio = b;
    ml.bytes = 0;
    ml.chunks = 0;
    if (mh != NULL)
        lh_MEM_doall_arg(mh, LHASH_DOALL_ARG_FN(print_leak), MEM_LEAK, &ml);
    if (ml.chunks != 0) {
        BIO_printf(b, "%ld bytes leaked in %d chunks\n", ml.bytes, ml.chunks);
#ifdef CRYPTO_MDEBUG_ABORT
        abort();
#endif
    } else {
        /*
         * Make sure that, if we found no leaks, memory-leak debugging itself
         * does not introduce memory leaks (which might irritate external
         * debugging tools). (When someone enables leak checking, but does not
         * call this function, we declare it to be their fault.) XXX This
         * should be in CRYPTO_mem_leaks_cb, and CRYPTO_mem_leaks should be
         * implemented by using CRYPTO_mem_leaks_cb. (Also there should be a
         * variant of lh_doall_arg that takes a function pointer instead of a
         * void *; this would obviate the ugly and illegal void_fn_to_char
         * kludge in CRYPTO_mem_leaks_cb. Otherwise the code police will come
         * and get us.)
         */
        int old_mh_mode;

        CRYPTO_w_lock(CRYPTO_LOCK_MALLOC);

        /*
         * avoid deadlock when lh_free() uses CRYPTO_dbg_free(), which uses
         * CRYPTO_is_mem_check_on
         */
        old_mh_mode = mh_mode;
        mh_mode = CRYPTO_MEM_CHECK_OFF;

        if (mh != NULL) {
            lh_MEM_free(mh);
            mh = NULL;
        }
        if (amih != NULL) {
            if (lh_APP_INFO_num_items(amih) == 0) {
                lh_APP_INFO_free(amih);
                amih = NULL;
            }
        }

        mh_mode = old_mh_mode;
        CRYPTO_w_unlock(CRYPTO_LOCK_MALLOC);
    }
    MemCheck_on();              /* release MALLOC2 lock */
}

#ifndef OPENSSL_NO_FP_API
void CRYPTO_mem_leaks_fp(FILE *fp)
{
    BIO *b;

    if (mh == NULL)
        return;
    /*
     * Need to turn off memory checking when allocated BIOs ... especially as
     * we're creating them at a time when we're trying to check we've not
     * left anything un-free()'d!!
     */
    MemCheck_off();
    b = BIO_new(BIO_s_file());
    MemCheck_on();
    if (!b)
        return;
    BIO_set_fp(b, fp, BIO_NOCLOSE);
    CRYPTO_mem_leaks(b);
    BIO_free(b);
}
#endif

/*
 * FIXME: We really don't allow much to the callback.  For example, it has no
 * chance of reaching the info stack for the item it processes.  Should it
 * really be this way? -- Richard Levitte
 */
/*
 * NB: The prototypes have been typedef'd to CRYPTO_MEM_LEAK_CB inside
 * crypto.h If this code is restructured, remove the callback type if it is
 * no longer needed. -- Geoff Thorpe
 */

/*
 * Can't pass CRYPTO_MEM_LEAK_CB directly to lh_MEM_doall_arg because it is a
 * function pointer and conversion to void * is prohibited. Instead pass its
 * address
 */

typedef CRYPTO_MEM_LEAK_CB *PCRYPTO_MEM_LEAK_CB;

static void cb_leak_doall_arg(const MEM *m, PCRYPTO_MEM_LEAK_CB *cb)
{
    (*cb) (m->order, m->file, m->line, m->num, m->addr);
}

static IMPLEMENT_LHASH_DOALL_ARG_FN(cb_leak, const MEM, PCRYPTO_MEM_LEAK_CB)

void CRYPTO_mem_leaks_cb(CRYPTO_MEM_LEAK_CB *cb)
{
    if (mh == NULL)
        return;
    CRYPTO_w_lock(CRYPTO_LOCK_MALLOC2);
    lh_MEM_doall_arg(mh, LHASH_DOALL_ARG_FN(cb_leak), PCRYPTO_MEM_LEAK_CB,
                     &cb);
    CRYPTO_w_unlock(CRYPTO_LOCK_MALLOC2);
}
