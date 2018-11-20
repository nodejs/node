/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>

int main(int argc, char **argv)
{
#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    char *p;
    char *lost;
    int noleak;

    p = getenv("OPENSSL_DEBUG_MEMORY");
    if (p != NULL && strcmp(p, "on") == 0)
        CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    lost = OPENSSL_malloc(3);
    if (lost == NULL) {
        fprintf(stderr, "OPENSSL_malloc failed\n");
        return 1;
    }

    if (argv[1] && strcmp(argv[1], "freeit") == 0) {
        OPENSSL_free(lost);
        lost = NULL;
    }

    noleak = CRYPTO_mem_leaks_fp(stderr);
    /* If -1 return value something bad happened */
    if (noleak == -1)
        return 1;
    return ((lost != NULL) ^ (noleak == 0));
#else
    return 0;
#endif
}
