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

#define OPENSSL_SUPPRESS_DEPRECATED /* EVP_PKEY_get1/set1_RSA */

#include <openssl/x509.h>
#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "crypto/x509.h" /* x509_st definition */
#include "testutil.h"

static char *certfile = NULL;
static int mcount, rcount, fcount, scount;

static int do_x509(int allow_failure)
{
    int ret = (allow_failure == 1) ? 0 : 1;
    BIO *bio = NULL;
    X509 *x509 = NULL;
    const ASN1_BIT_STRING *sig = NULL;
    const X509_ALGOR *alg = NULL;
    EVP_PKEY *pkey;
#ifndef OPENSSL_NO_DEPRECATED_3_0
    RSA *rsa = NULL;
#endif

    if (!TEST_ptr(bio = BIO_new_file(certfile, "r"))
        || !TEST_ptr(x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL))
        || !TEST_ptr(pkey = X509_get0_pubkey(x509)))
        goto err;

#ifndef OPENSSL_NO_DEPRECATED_3_0
    /* Issue #24575 requires legacy key but the test is useful anyway */
    if (!TEST_ptr(rsa = EVP_PKEY_get1_RSA(pkey)))
        goto err;

    if (!TEST_int_gt(EVP_PKEY_set1_RSA(pkey, rsa), 0))
        goto err;
#endif

    X509_get0_signature(&sig, &alg, x509);

    if (!TEST_int_gt(ASN1_item_verify(ASN1_ITEM_rptr(X509_CINF),
                                      (X509_ALGOR *)alg, (ASN1_BIT_STRING *)sig,
                                      &x509->cert_info, pkey), 0))
        goto err;

    if (!TEST_int_lt(ASN1_item_verify(ASN1_ITEM_rptr(X509_CINF),
                                      (X509_ALGOR *)alg, (ASN1_BIT_STRING *)sig,
                                      NULL, pkey), 0))
        goto err;

    X509_issuer_name_hash(x509);

    ret = 1;

 err:
#ifndef OPENSSL_NO_DEPRECATED_3_0
    RSA_free(rsa);
#endif
    X509_free(x509);
    BIO_free(bio);
    return ret;
}

static int test_record_alloc_counts(void)
{
    return do_x509(1);
}

static int test_alloc_failures(void)
{
    return do_x509(0);
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
    int ret = 0;
    char *opmode = NULL;

    if (!TEST_ptr(opmode = test_get_argument(0)))
        goto err;

    if (!TEST_ptr(certfile = test_get_argument(1)))
        goto err;

    if (strcmp(opmode, "count") == 0) {
        CRYPTO_get_alloc_counts(&scount, &rcount, &fcount);
        ADD_TEST(test_record_alloc_counts);
        ADD_TEST(test_report_alloc_counts);
    } else {
        ADD_TEST(test_alloc_failures);
    }
    ret = 1;
err:
    return ret;
}

void cleanup_tests(void)
{
}
