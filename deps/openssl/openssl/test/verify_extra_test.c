/*
 * Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "testutil.h"

static const char *certs_dir;
static char *roots_f = NULL;
static char *untrusted_f = NULL;
static char *bad_f = NULL;
static char *good_f = NULL;
static char *sroot_cert = NULL;
static char *ca_cert = NULL;
static char *ee_cert = NULL;

static X509 *load_cert_pem(const char *file)
{
    X509 *cert = NULL;
    BIO *bio = NULL;

    if (!TEST_ptr(bio = BIO_new(BIO_s_file())))
        return NULL;
    if (TEST_int_gt(BIO_read_filename(bio, file), 0))
        (void)TEST_ptr(cert = PEM_read_bio_X509(bio, NULL, NULL, NULL));

    BIO_free(bio);
    return cert;
}

static STACK_OF(X509) *load_certs_from_file(const char *filename)
{
    STACK_OF(X509) *certs;
    BIO *bio;
    X509 *x;

    bio = BIO_new_file(filename, "r");

    if (bio == NULL) {
        return NULL;
    }

    certs = sk_X509_new_null();
    if (certs == NULL) {
        BIO_free(bio);
        return NULL;
    }

    ERR_set_mark();
    do {
        x = PEM_read_bio_X509(bio, NULL, 0, NULL);
        if (x != NULL && !sk_X509_push(certs, x)) {
            sk_X509_pop_free(certs, X509_free);
            BIO_free(bio);
            return NULL;
        } else if (x == NULL) {
            /*
             * We probably just ran out of certs, so ignore any errors
             * generated
             */
            ERR_pop_to_mark();
        }
    } while (x != NULL);

    BIO_free(bio);

    return certs;
}

/*-
 * Test for CVE-2015-1793 (Alternate Chains Certificate Forgery)
 *
 * Chain is as follows:
 *
 * rootCA (self-signed)
 *   |
 * interCA
 *   |
 * subinterCA       subinterCA (self-signed)
 *   |                   |
 * leaf ------------------
 *   |
 * bad
 *
 * rootCA, interCA, subinterCA, subinterCA (ss) all have CA=TRUE
 * leaf and bad have CA=FALSE
 *
 * subinterCA and subinterCA (ss) have the same subject name and keys
 *
 * interCA (but not rootCA) and subinterCA (ss) are in the trusted store
 * (roots.pem)
 * leaf and subinterCA are in the untrusted list (untrusted.pem)
 * bad is the certificate being verified (bad.pem)
 *
 * Versions vulnerable to CVE-2015-1793 will fail to detect that leaf has
 * CA=FALSE, and will therefore incorrectly verify bad
 *
 */
static int test_alt_chains_cert_forgery(void)
{
    int ret = 0;
    int i;
    X509 *x = NULL;
    STACK_OF(X509) *untrusted = NULL;
    BIO *bio = NULL;
    X509_STORE_CTX *sctx = NULL;
    X509_STORE *store = NULL;
    X509_LOOKUP *lookup = NULL;

    store = X509_STORE_new();
    if (store == NULL)
        goto err;

    lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
    if (lookup == NULL)
        goto err;
    if (!X509_LOOKUP_load_file(lookup, roots_f, X509_FILETYPE_PEM))
        goto err;

    untrusted = load_certs_from_file(untrusted_f);

    if ((bio = BIO_new_file(bad_f, "r")) == NULL)
        goto err;

    if ((x = PEM_read_bio_X509(bio, NULL, 0, NULL)) == NULL)
        goto err;

    sctx = X509_STORE_CTX_new();
    if (sctx == NULL)
        goto err;

    if (!X509_STORE_CTX_init(sctx, store, x, untrusted))
        goto err;

    i = X509_verify_cert(sctx);

    if (i != 0 || X509_STORE_CTX_get_error(sctx) != X509_V_ERR_INVALID_CA)
        goto err;

    /* repeat with X509_V_FLAG_X509_STRICT */
    X509_STORE_CTX_cleanup(sctx);
    X509_STORE_set_flags(store, X509_V_FLAG_X509_STRICT);

    if (!X509_STORE_CTX_init(sctx, store, x, untrusted))
        goto err;

    i = X509_verify_cert(sctx);

    if (i == 0 && X509_STORE_CTX_get_error(sctx) == X509_V_ERR_INVALID_CA)
        /* This is the result we were expecting: Test passed */
        ret = 1;

 err:
    X509_STORE_CTX_free(sctx);
    X509_free(x);
    BIO_free(bio);
    sk_X509_pop_free(untrusted, X509_free);
    X509_STORE_free(store);
    return ret;
}

static int test_store_ctx(void)
{
    X509_STORE_CTX *sctx = NULL;
    X509 *x = NULL;
    BIO *bio = NULL;
    int testresult = 0, ret;

    bio = BIO_new_file(bad_f, "r");
    if (bio == NULL)
        goto err;

    x = PEM_read_bio_X509(bio, NULL, 0, NULL);
    if (x == NULL)
        goto err;

    sctx = X509_STORE_CTX_new();
    if (sctx == NULL)
        goto err;

    if (!X509_STORE_CTX_init(sctx, NULL, x, NULL))
        goto err;

    /* Verifying a cert where we have no trusted certs should fail */
    ret = X509_verify_cert(sctx);

    if (ret == 0) {
        /* This is the result we were expecting: Test passed */
        testresult = 1;
    }

 err:
    X509_STORE_CTX_free(sctx);
    X509_free(x);
    BIO_free(bio);
    return testresult;
}

static int test_self_signed(const char *filename, int expected)
{
    X509 *cert = load_cert_pem(filename);
    STACK_OF(X509) *trusted = sk_X509_new_null();
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    int ret;

    ret = TEST_ptr(cert)
        && TEST_true(sk_X509_push(trusted, cert))
        && TEST_true(X509_STORE_CTX_init(ctx, NULL, cert, NULL));
    X509_STORE_CTX_set0_trusted_stack(ctx, trusted);
    ret = ret && TEST_int_eq(X509_verify_cert(ctx), expected);

    X509_STORE_CTX_free(ctx);
    sk_X509_free(trusted);
    X509_free(cert);
    return ret;
}

static int test_self_signed_good(void)
{
    return test_self_signed(good_f, 1);
}

static int test_self_signed_bad(void)
{
    return test_self_signed(bad_f, 0);
}

static int do_test_purpose(int purpose, int expected)
{
    X509 *eecert = load_cert_pem(ee_cert); /* may result in NULL */
    X509 *untrcert = load_cert_pem(ca_cert);
    X509 *trcert = load_cert_pem(sroot_cert);
    STACK_OF(X509) *trusted = sk_X509_new_null();
    STACK_OF(X509) *untrusted = sk_X509_new_null();
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    int testresult = 0;

    if (!TEST_ptr(eecert)
            || !TEST_ptr(untrcert)
            || !TEST_ptr(trcert)
            || !TEST_ptr(trusted)
            || !TEST_ptr(untrusted)
            || !TEST_ptr(ctx))
        goto err;


    if (!TEST_true(sk_X509_push(trusted, trcert)))
        goto err;
    trcert = NULL;
    if (!TEST_true(sk_X509_push(untrusted, untrcert)))
        goto err;
    untrcert = NULL;

    if (!TEST_true(X509_STORE_CTX_init(ctx, NULL, eecert, untrusted)))
        goto err;

    if (!TEST_true(X509_STORE_CTX_set_purpose(ctx, purpose)))
        goto err;

    /*
     * X509_STORE_CTX_set0_trusted_stack() is bady named. Despite the set0 name
     * we are still responsible for freeing trusted after we have finished with
     * it.
     */
    X509_STORE_CTX_set0_trusted_stack(ctx, trusted);

    if (!TEST_int_eq(X509_verify_cert(ctx), expected))
        goto err;

    testresult = 1;
 err:
    sk_X509_pop_free(trusted, X509_free);
    sk_X509_pop_free(untrusted, X509_free);
    X509_STORE_CTX_free(ctx);
    X509_free(eecert);
    X509_free(untrcert);
    X509_free(trcert);
    return testresult;
}

static int test_purpose_ssl_client(void)
{
    return do_test_purpose(X509_PURPOSE_SSL_CLIENT, 0);
}

static int test_purpose_ssl_server(void)
{
    return do_test_purpose(X509_PURPOSE_SSL_SERVER, 1);
}

static int test_purpose_any(void)
{
    return do_test_purpose(X509_PURPOSE_ANY, 1);
}

int setup_tests(void)
{
    if (!TEST_ptr(certs_dir = test_get_argument(0))) {
        TEST_error("usage: verify_extra_test certs-dir\n");
        return 0;
    }

    if (!TEST_ptr(roots_f = test_mk_file_path(certs_dir, "roots.pem"))
            || !TEST_ptr(untrusted_f = test_mk_file_path(certs_dir, "untrusted.pem"))
            || !TEST_ptr(bad_f = test_mk_file_path(certs_dir, "bad.pem"))
            || !TEST_ptr(good_f = test_mk_file_path(certs_dir, "rootCA.pem"))
            || !TEST_ptr(sroot_cert = test_mk_file_path(certs_dir, "sroot-cert.pem"))
            || !TEST_ptr(ca_cert = test_mk_file_path(certs_dir, "ca-cert.pem"))
            || !TEST_ptr(ee_cert = test_mk_file_path(certs_dir, "ee-cert.pem")))
        goto err;

    ADD_TEST(test_alt_chains_cert_forgery);
    ADD_TEST(test_store_ctx);
    ADD_TEST(test_self_signed_good);
    ADD_TEST(test_self_signed_bad);
    ADD_TEST(test_purpose_ssl_client);
    ADD_TEST(test_purpose_ssl_server);
    ADD_TEST(test_purpose_any);
    return 1;
 err:
    cleanup_tests();
    return 0;
}

void cleanup_tests(void)
{
    OPENSSL_free(roots_f);
    OPENSSL_free(untrusted_f);
    OPENSSL_free(bad_f);
    OPENSSL_free(good_f);
    OPENSSL_free(sroot_cert);
    OPENSSL_free(ca_cert);
    OPENSSL_free(ee_cert);
}
