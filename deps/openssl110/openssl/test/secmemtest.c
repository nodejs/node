/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>

#define perror_line()    perror_line1(__LINE__)
#define perror_line1(l)  perror_line2(l)
#define perror_line2(l)  perror("failed " #l)

int main(int argc, char **argv)
{
#if defined(OPENSSL_SYS_LINUX) || defined(OPENSSL_SYS_UNIX)
    char *p = NULL, *q = NULL, *r = NULL, *s = NULL;

    r = OPENSSL_secure_malloc(20);
    /* r = non-secure 20 */
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
    /* r = non-secure 20, p = secure 20 */
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
    /* r = non-secure 20, p = secure 20, q = non-secure 20 */
    if (CRYPTO_secure_allocated(q)) {
        perror_line();
        return 1;
    }
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
    OPENSSL_secure_free(p);
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
