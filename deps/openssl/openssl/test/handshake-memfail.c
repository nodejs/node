/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include "helpers/ssltestlib.h"
#include "testutil.h"

/**
 * @brief Global static variables for certificate and key handling.
 *
 * These variables store the paths and context used for managing
 * certificates and private keys in the application.
 *
 * - certsdir: Directory containing trusted certificates.
 * - cert:     Path to the certificate file in use.
 * - privkey:  Path to the private key file.
 * - mcount:   Number of mallocs counted
 * - rcount:   Number of reallocs counted
 * - fcount:   Number of frees counted
 * - scount:   Number of mallocs counted prior to workload
 */
static char *cert = NULL;
static char *privkey = NULL;
static int mcount, rcount, fcount, scount;

/**
 * @brief Performs an SSL/TLS handshake between a test client and server.
 *
 * This function sets up SSL/TLS contexts and objects for both client and
 * server, then initiates a handshake to verify successful connection
 * establishment. It is intended for use in testing scenarios to validate
 * handshake behavior using specified certificates and keys.
 *
 * @return 1 on successful handshake, 0 on failure.
 *
 * @note The function uses @c TEST_true() macros to validate intermediate
 *       steps. All SSL objects and contexts are freed before returning.
 */
static int do_handshake(OSSL_LIB_CTX *libctx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;

    if (!TEST_true(create_ssl_ctx_pair(libctx, TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION, 0,
                                       &sctx, &cctx, cert, privkey)))
        return 0;

    /* Now do a handshake */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    testresult = 1;

end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

/**
 * @brief run our workload to count the number of allocations we make.
 *
 * Creates a new OpenSSL library context and performs a test SSL/TLS
 * handshake. The number of malloc operations is recorded and printed for
 * diagnostic purposes.
 *
 * @return 1 if the handshake succeeds, 0 otherwise.
 */
static int test_record_alloc_counts(void)
{
    int ret;
    OSSL_LIB_CTX *libctx;

    libctx = OSSL_LIB_CTX_new();
    if (!TEST_ptr(libctx))
        return 0;

    ret = do_handshake(libctx);

    OSSL_LIB_CTX_free(libctx);
    libctx = NULL;

    return ret;
}

/**
 * @brief run our workload to count the number of allocations we make.
 *
 * Creates a new OpenSSL library context and performs a test SSL/TLS
 * handshake.
 *
 * Note this is exactly the same as test_record_alloc_counts with 1 difference
 * The test always returns 1.  We do this because with allocation failures
 * in effect, we can't expect things to work, so we always return success
 * so that the test keeps running.
 */
static int test_alloc_failures(void)
{
    OSSL_LIB_CTX *libctx;

    libctx = OSSL_LIB_CTX_new();
    if (!TEST_ptr(libctx))
        return 1;

    do_handshake(libctx);

    OSSL_LIB_CTX_free(libctx);
    libctx = NULL;

    return 1;
}

static int test_report_alloc_counts(void)
{
    CRYPTO_get_alloc_counts(&mcount, &rcount, &fcount);
    /*
     * Report our memory allocations from the count run
     * NOTE: We report a number of allocations to skip here
     * (the scount value).  These are the allocations that took
     * place while the test harness itself was getting setup
     * (i.e. calling OPENSSL_init_crypto/etc).  We can't fail
     * those allocations as they will cause the test to fail before
     * we have even run the workload.  So report them so we can
     * allow them to function before we start doing any real testing
     */
    TEST_info("skip: %d count %d\n", scount, mcount - scount);
    return 1;
}

int setup_tests(void)
{
    char *opmode = NULL;
    char *certsdir = NULL;

    if (!TEST_ptr(opmode = test_get_argument(0)))
        goto err;

    if (!TEST_ptr(certsdir = test_get_argument(1)))
        goto err;

    cert = test_mk_file_path(certsdir, "servercert.pem");
    if (cert == NULL)
        goto err;

    privkey = test_mk_file_path(certsdir, "serverkey.pem");
    if (privkey == NULL)
        goto err;

    if (strcmp(opmode, "count") == 0) {
        CRYPTO_get_alloc_counts(&scount, &rcount, &fcount);
        ADD_TEST(test_record_alloc_counts);
        ADD_TEST(test_report_alloc_counts);
    } else {
        ADD_TEST(test_alloc_failures);
    }
    return 1;

 err:
    OPENSSL_free(cert);
    OPENSSL_free(privkey);
    return 0;
}

void cleanup_tests(void)
{
    OPENSSL_free(cert);
    OPENSSL_free(privkey);
}
