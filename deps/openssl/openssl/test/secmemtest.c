/*
 * Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/crypto.h>

#define perror_line()    perror_line1(__LINE__)
#define perror_line1(l)  perror_line2(l)
#define perror_line2(l)  perror("failed " #l)

int main(int argc, char **argv)
{
#if defined(OPENSSL_SYS_LINUX) || defined(OPENSSL_SYS_UNIX)
    char *p = NULL, *q = NULL, *r = NULL, *s = NULL;
    int i;
    const int size = 64;

    s = OPENSSL_secure_malloc(20);
    /* s = non-secure 20 */
    if (s == NULL) {
        perror_line();
        return 1;
    }
    if (CRYPTO_secure_allocated(s)) {
        perror_line();
        return 1;
    }
    r = OPENSSL_secure_malloc(20);
    /* r = non-secure 20, s = non-secure 20 */
    if (r == NULL) {
        perror_line();
        return 1;
    }
    if (!CRYPTO_secure_malloc_init(4096, 32)) {
        perror_line();
        return 1;
    }
    if (CRYPTO_secure_allocated(r)) {
        perror_line();
        return 1;
    }
    p = OPENSSL_secure_malloc(20);
    /* r = non-secure 20, p = secure 20, s = non-secure 20 */
    if (!CRYPTO_secure_allocated(p)) {
        perror_line();
        return 1;
    }
    /* 20 secure -> 32-byte minimum allocaton unit */
    if (CRYPTO_secure_used() != 32) {
        perror_line();
        return 1;
    }
    q = OPENSSL_malloc(20);
    /* r = non-secure 20, p = secure 20, q = non-secure 20, s = non-secure 20 */
    if (CRYPTO_secure_allocated(q)) {
        perror_line();
        return 1;
    }
    OPENSSL_secure_clear_free(s, 20);
    s = OPENSSL_secure_malloc(20);
    /* r = non-secure 20, p = secure 20, q = non-secure 20, s = secure 20 */
    if (!CRYPTO_secure_allocated(s)) {
        perror_line();
        return 1;
    }
    /* 2 * 20 secure -> 64 bytes allocated */
    if (CRYPTO_secure_used() != 64) {
        perror_line();
        return 1;
    }
    OPENSSL_secure_clear_free(p, 20);
    /* 20 secure -> 32 bytes allocated */
    if (CRYPTO_secure_used() != 32) {
        perror_line();
        return 1;
    }
    OPENSSL_free(q);
    /* should not complete, as secure memory is still allocated */
    if (CRYPTO_secure_malloc_done()) {
        perror_line();
        return 1;
    }
    if (!CRYPTO_secure_malloc_initialized()) {
        perror_line();
        return 1;
    }
    OPENSSL_secure_free(s);
    /* secure memory should now be 0, so done should complete */
    if (CRYPTO_secure_used() != 0) {
        perror_line();
        return 1;
    }
    if (!CRYPTO_secure_malloc_done()) {
        perror_line();
        return 1;
    }
    if (CRYPTO_secure_malloc_initialized()) {
        perror_line();
        return 1;
    }

    fprintf(stderr, "Possible infinite loop: allocate more than available\n");
    if (!CRYPTO_secure_malloc_init(32768, 16)) {
        perror_line();
        return 1;
    }
    if (OPENSSL_secure_malloc((size_t)-1) != NULL) {
        perror_line();
        return 1;
    }
    if (!CRYPTO_secure_malloc_done()) {
        perror_line();
        return 1;
    }

    /*
     * If init fails, then initialized should be false, if not, this
     * could cause an infinite loop secure_malloc, but we don't test it
     */
    if (!CRYPTO_secure_malloc_init(16, 16) &&
        CRYPTO_secure_malloc_initialized()) {
        CRYPTO_secure_malloc_done();
        perror_line();
        return 1;
    }

    if (!CRYPTO_secure_malloc_init(32768, 16)) {
        perror_line();
        return 1;
    }

    /*
     * Verify that secure memory gets zeroed properly.
     */
    if ((p = OPENSSL_secure_malloc(size)) == NULL) {
        perror_line();
        return 1;
    }
    for (i = 0; i < size; i++)
        if (p[i] != 0) {
            perror_line();
            fprintf(stderr, "iteration %d\n", i);
            return 1;
        }

    for (i = 0; i < size; i++)
        p[i] = (unsigned char)(i + ' ' + 1);
    OPENSSL_secure_free(p);

    /*
     * A deliberate use after free here to verify that the memory has been
     * cleared properly.  Since secure free doesn't return the memory to
     * libc's memory pool, it technically isn't freed.  However, the header
     * bytes have to be skipped and these consist of two pointers in the
     * current implementation.
     */
    for (i = sizeof(void *) * 2; i < size; i++)
        if (p[i] != 0) {
            perror_line();
            fprintf(stderr, "iteration %d\n", i);
            return 1;
        }

    if (!CRYPTO_secure_malloc_done()) {
        perror_line();
        return 1;
    }

    /*-
     * There was also a possible infinite loop when the number of
     * elements was 1<<31, as |int i| was set to that, which is a
     * negative number. However, it requires minimum input values:
     *
     * CRYPTO_secure_malloc_init((size_t)1<<34, (size_t)1<<4);
     *
     * Which really only works on 64-bit systems, since it took 16 GB
     * secure memory arena to trigger the problem. It naturally takes
     * corresponding amount of available virtual and physical memory
     * for test to be feasible/representative. Since we can't assume
     * that every system is equipped with that much memory, the test
     * remains disabled. If the reader of this comment really wants
     * to make sure that infinite loop is fixed, they can enable the
     * code below.
     */
# if 0
    /*-
     * On Linux and BSD this test has a chance to complete in minimal
     * time and with minimum side effects, because mlock is likely to
     * fail because of RLIMIT_MEMLOCK, which is customarily [much]
     * smaller than 16GB. In other words Linux and BSD users can be
     * limited by virtual space alone...
     */
    if (sizeof(size_t) > 4) {
        fprintf(stderr, "Possible infinite loop: 1<<31 limit\n");
        if (CRYPTO_secure_malloc_init((size_t)1<<34, (size_t)1<<4) == 0) {
            perror_line();
        } else if (!CRYPTO_secure_malloc_done()) {
            perror_line();
            return 1;
        }
    }
# endif

    /* this can complete - it was not really secure */
    OPENSSL_secure_free(r);
#else
    /* Should fail. */
    if (CRYPTO_secure_malloc_init(4096, 32)) {
        perror_line();
        return 1;
    }
#endif
    return 0;
}
