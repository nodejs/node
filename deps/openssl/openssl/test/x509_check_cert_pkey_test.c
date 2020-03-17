/*
 * Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>

#include <openssl/pem.h>
#include <openssl/x509.h>
#include "testutil.h"

/*
 * c: path of a cert in PEM format
 * k: path of a key in PEM format
 * t: API type, "cert" for X509_ and "req" for X509_REQ_ APIs.
 * e: expected, "ok" for success, "failed" for what should fail.
 */
static const char *c;
static const char *k;
static const char *t;
static const char *e;

static int test_x509_check_cert_pkey(void)
{
    BIO *bio = NULL;
    X509 *x509 = NULL;
    X509_REQ *x509_req = NULL;
    EVP_PKEY *pkey = NULL;
    int ret = 0, type = 0, expected = 0, result = 0;

    /*
     * we check them first thus if fails we don't need to do
     * those PEM parsing operations.
     */
    if (strcmp(t, "cert") == 0) {
        type = 1;
    } else if (strcmp(t, "req") == 0) {
        type = 2;
    } else {
        TEST_error("invalid 'type'");
        goto failed;
    }

    if (strcmp(e, "ok") == 0) {
        expected = 1;
    } else if (strcmp(e, "failed") == 0) {
        expected = 0;
    } else {
        TEST_error("invalid 'expected'");
        goto failed;
    }

    /* process private key */
    if (!TEST_ptr(bio = BIO_new_file(k, "r")))
        goto failed;

    if (!TEST_ptr(pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL)))
        goto failed;

    BIO_free(bio);

    /* process cert or cert request, use the same local var */
    if (!TEST_ptr(bio = BIO_new_file(c, "r")))
        goto failed;

    switch (type) {
    case 1:
        x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        if (x509 == NULL) {
            TEST_error("read PEM x509 failed");
            goto failed;
        }

        result = X509_check_private_key(x509, pkey);
        break;
    case 2:
        x509_req = PEM_read_bio_X509_REQ(bio, NULL, NULL, NULL);
        if (x509_req == NULL) {
            TEST_error("read PEM x509 req failed");
            goto failed;
        }

        result = X509_REQ_check_private_key(x509_req, pkey);
        break;
    default:
        /* should never be here */
        break;
    }

    if (!TEST_int_eq(result, expected)) {
        TEST_error("check private key: expected: %d, got: %d", expected, result);
        goto failed;
    }

    ret = 1;
failed:
    BIO_free(bio);
    X509_free(x509);
    X509_REQ_free(x509_req);
    EVP_PKEY_free(pkey);
    return ret;
}

int setup_tests(void)
{
    if (!TEST_ptr(c = test_get_argument(0))
            || !TEST_ptr(k = test_get_argument(1))
            || !TEST_ptr(t = test_get_argument(2))
            || !TEST_ptr(e = test_get_argument(3))) {
        TEST_note("usage: x509_check_cert_pkey cert.pem|cert.req"
                  " key.pem cert|req <expected>");
        return 0;
    }

    ADD_TEST(test_x509_check_cert_pkey);
    return 1;
}
