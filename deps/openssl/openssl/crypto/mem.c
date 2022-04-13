/*
 * Copyright 1995-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "e_os.h"
#include "internal/cryptlib.h"
#include "crypto/cryptlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <openssl/crypto.h>

/*
 * the following pointers may be changed as long as 'allow_customize' is set
 */
static int allow_customize = 1;
static CRYPTO_malloc_fn malloc_impl = CRYPTO_malloc;
static CRYPTO_realloc_fn realloc_impl = CRYPTO_realloc;
static CRYPTO_free_fn free_impl = CRYPTO_free;

#if !defined(OPENSSL_NO_CRYPTO_MDEBUG) && !defined(FIPS_MODULE)
# include "internal/tsan_assist.h"

# ifdef TSAN_REQUIRES_LOCKING
#  define INCREMENT(x) /* empty */
#  define LOAD(x) 0
# else  /* TSAN_REQUIRES_LOCKING */
static TSAN_QUALIFIER int malloc_count;
static TSAN_QUALIFIER int realloc_count;
static TSAN_QUALIFIER int free_count;

#  define INCREMENT(x) tsan_counter(&(x))
#  define LOAD(x)      tsan_load(&x)
# endif /* TSAN_REQUIRES_LOCKING */

static char *md_failstring;
static long md_count;
static int md_fail_percent = 0;
static int md_tracefd = -1;

static void parseit(void);
static int shouldfail(void);

# define FAILTEST() if (shouldfail()) return NULL

#else

# define INCREMENT(x) /* empty */
# define FAILTEST() /* empty */
#endif

int CRYPTO_set_mem_functions(CRYPTO_malloc_fn malloc_fn,
                             CRYPTO_realloc_fn realloc_fn,
                             CRYPTO_free_fn free_fn)
{
    if (!allow_customize)
        return 0;
    if (malloc_fn != NULL)
        malloc_impl = malloc_fn;
    if (realloc_fn != NULL)
        realloc_impl = realloc_fn;
    if (free_fn != NULL)
        free_impl = free_fn;
    return 1;
}

void CRYPTO_get_mem_functions(CRYPTO_malloc_fn *malloc_fn,
                              CRYPTO_realloc_fn *realloc_fn,
                              CRYPTO_free_fn *free_fn)
{
    if (malloc_fn != NULL)
        *malloc_fn = malloc_impl;
    if (realloc_fn != NULL)
        *realloc_fn = realloc_impl;
    if (free_fn != NULL)
        *free_fn = free_impl;
}

#if !defined(OPENSSL_NO_CRYPTO_MDEBUG) && !defined(FIPS_MODULE)
void CRYPTO_get_alloc_counts(int *mcount, int *rcount, int *fcount)
{
    if (mcount != NULL)
        *mcount = LOAD(malloc_count);
    if (rcount != NULL)
        *rcount = LOAD(realloc_count);
    if (fcount != NULL)
        *fcount = LOAD(free_count);
}

/*
 * Parse a "malloc failure spec" string.  This likes like a set of fields
 * separated by semicolons.  Each field has a count and an optional failure
 * percentage.  For example:
 *          100@0;100@25;0@0
 *    or    100;100@25;0
 * This means 100 mallocs succeed, then next 100 fail 25% of the time, and
 * all remaining (count is zero) succeed.
 */
static void parseit(void)
{
    char *semi = strchr(md_failstring, ';');
    char *atsign;

    if (semi != NULL)
        *semi++ = '\0';

    /* Get the count (atol will stop at the @ if there), and percentage */
    md_count = atol(md_failstring);
    atsign = strchr(md_failstring, '@');
    md_fail_percent = atsign == NULL ? 0 : atoi(atsign + 1);

    if (semi != NULL)
        md_failstring = semi;
}

/*
 * Windows doesn't have random(), but it has rand()
 * Some rand() implementations aren't good, but we're not
 * dealing with secure randomness here.
 */
# ifdef _WIN32
#  define random() rand()
# endif
/*
 * See if the current malloc should fail.
 */
static int shouldfail(void)
{
    int roll = (int)(random() % 100);
    int shoulditfail = roll < md_fail_percent;
# ifndef _WIN32
/* suppressed on Windows as POSIX-like file descriptors are non-inheritable */
    int len;
    char buff[80];

    if (md_tracefd > 0) {
        BIO_snprintf(buff, sizeof(buff),
                     "%c C%ld %%%d R%d\n",
                     shoulditfail ? '-' : '+', md_count, md_fail_percent, roll);
        len = strlen(buff);
        if (write(md_tracefd, buff, len) != len)
            perror("shouldfail write failed");
    }
# endif

    if (md_count) {
        /* If we used up this one, go to the next. */
        if (--md_count == 0)
            parseit();
    }

    return shoulditfail;
}

void ossl_malloc_setup_failures(void)
{
    const char *cp = getenv("OPENSSL_MALLOC_FAILURES");

    if (cp != NULL && (md_failstring = strdup(cp)) != NULL)
        parseit();
    if ((cp = getenv("OPENSSL_MALLOC_FD")) != NULL)
        md_tracefd = atoi(cp);
}
#endif

void *CRYPTO_malloc(size_t num, const char *file, int line)
{
    INCREMENT(malloc_count);
    if (malloc_impl != CRYPTO_malloc)
        return malloc_impl(num, file, line);

    if (num == 0)
        return NULL;

    FAILTEST();
    if (allow_customize) {
        /*
         * Disallow customization after the first allocation. We only set this
         * if necessary to avoid a store to the same cache line on every
         * allocation.
         */
        allow_customize = 0;
    }

    return malloc(num);
}

void *CRYPTO_zalloc(size_t num, const char *file, int line)
{
    void *ret;

    ret = CRYPTO_malloc(num, file, line);
    FAILTEST();
    if (ret != NULL)
        memset(ret, 0, num);

    return ret;
}

void *CRYPTO_realloc(void *str, size_t num, const char *file, int line)
{
    INCREMENT(realloc_count);
    if (realloc_impl != CRYPTO_realloc)
        return realloc_impl(str, num, file, line);

    FAILTEST();
    if (str == NULL)
        return CRYPTO_malloc(num, file, line);

    if (num == 0) {
        CRYPTO_free(str, file, line);
        return NULL;
    }

    return realloc(str, num);
}

void *CRYPTO_clear_realloc(void *str, size_t old_len, size_t num,
                           const char *file, int line)
{
    void *ret = NULL;

    if (str == NULL)
        return CRYPTO_malloc(num, file, line);

    if (num == 0) {
        CRYPTO_clear_free(str, old_len, file, line);
        return NULL;
    }

    /* Can't shrink the buffer since memcpy below copies |old_len| bytes. */
    if (num < old_len) {
        OPENSSL_cleanse((char*)str + num, old_len - num);
        return str;
    }

    ret = CRYPTO_malloc(num, file, line);
    if (ret != NULL) {
        memcpy(ret, str, old_len);
        CRYPTO_clear_free(str, old_len, file, line);
    }
    return ret;
}

void CRYPTO_free(void *str, const char *file, int line)
{
    INCREMENT(free_count);
    if (free_impl != CRYPTO_free) {
        free_impl(str, file, line);
        return;
    }

    free(str);
}

void CRYPTO_clear_free(void *str, size_t num, const char *file, int line)
{
    if (str == NULL)
        return;
    if (num)
        OPENSSL_cleanse(str, num);
    CRYPTO_free(str, file, line);
}

#if !defined(OPENSSL_NO_CRYPTO_MDEBUG)

# ifndef OPENSSL_NO_DEPRECATED_3_0
int CRYPTO_mem_ctrl(int mode)
{
    (void)mode;
    return -1;
}

int CRYPTO_set_mem_debug(int flag)
{
    (void)flag;
    return -1;
}

int CRYPTO_mem_debug_push(const char *info, const char *file, int line)
{
    (void)info; (void)file; (void)line;
    return -1;
}

int CRYPTO_mem_debug_pop(void)
{
    return -1;
}

void CRYPTO_mem_debug_malloc(void *addr, size_t num, int flag,
                             const char *file, int line)
{
    (void)addr; (void)num; (void)flag; (void)file; (void)line;
}

void CRYPTO_mem_debug_realloc(void *addr1, void *addr2, size_t num, int flag,
                              const char *file, int line)
{
    (void)addr1; (void)addr2; (void)num; (void)flag; (void)file; (void)line;
}

void CRYPTO_mem_debug_free(void *addr, int flag,
                           const char *file, int line)
{
    (void)addr; (void)flag; (void)file; (void)line;
}

int CRYPTO_mem_leaks(BIO *b)
{
    (void)b;
    return -1;
}

#  ifndef OPENSSL_NO_STDIO
int CRYPTO_mem_leaks_fp(FILE *fp)
{
    (void)fp;
    return -1;
}
#  endif

int CRYPTO_mem_leaks_cb(int (*cb)(const char *str, size_t len, void *u),
                        void *u)
{
    (void)cb; (void)u;
    return -1;
}

# endif

#endif
