/*
 * Copyright 2016-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>

#include "testutil.h"

/*
 * We use a proper main function here instead of the custom main from the
 * test framework because the CRYPTO_mem_leaks_fp function cannot be called
 * a second time without trying to use a null pointer.  The test framework
 * calls this function as part of its close down.
 *
 * A work around is to call putenv("OPENSSL_DEBUG_MEMORY=0"); before exiting
 * but that is worse than avoiding the test framework's main.
 */

int main(int argc, char *argv[])
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
    if (!TEST_ptr(lost))
        return EXIT_FAILURE;

    if (argv[1] && strcmp(argv[1], "freeit") == 0) {
        OPENSSL_free(lost);
        lost = NULL;
    }

    noleak = CRYPTO_mem_leaks_fp(stderr);
    /* If -1 return value something bad happened */
    if (!TEST_int_ne(noleak, -1))
        return EXIT_FAILURE;

    return TEST_int_eq(lost != NULL, noleak == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    return EXIT_SUCCESS;
#endif
}
