/*
 * Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/ocsp.h>

#include "ssltestlib.h"
#include "testutil.h"
#include "e_os.h"

static char *cert = NULL;
static char *privkey = NULL;

#ifndef OPENSSL_NO_OCSP
static const unsigned char orespder[] = "Dummy OCSP Response";
static int ocsp_server_called = 0;
static int ocsp_client_called = 0;

static int cdummyarg = 1;
static X509 *ocspcert = NULL;
#endif

#define NUM_EXTRA_CERTS 40

static int execute_test_large_message(const SSL_METHOD *smeth,
                                      const SSL_METHOD *cmeth,
                                      int min_version, int max_version,
                                      int read_ahead)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    int i;
    BIO *certbio = BIO_new_file(cert, "r");
    X509 *chaincert = NULL;
    int certlen;

    if (certbio == NULL) {
        printf("Can't load the certificate file\n");
        goto end;
    }
    chaincert = PEM_read_bio_X509(certbio, NULL, NULL, NULL);
    BIO_free(certbio);
    certbio = NULL;
    if (chaincert == NULL) {
        printf("Unable to load certificate for chain\n");
        goto end;
    }

    if (!create_ssl_ctx_pair(smeth, cmeth, min_version, max_version, &sctx,
                             &cctx, cert, privkey)) {
        printf("Unable to create SSL_CTX pair\n");
        goto end;
    }

    if(read_ahead) {
        /*
         * Test that read_ahead works correctly when dealing with large
         * records
         */
        SSL_CTX_set_read_ahead(cctx, 1);
    }

    /*
     * We assume the supplied certificate is big enough so that if we add
     * NUM_EXTRA_CERTS it will make the overall message large enough. The
     * default buffer size is requested to be 16k, but due to the way BUF_MEM
     * works, it ends up allocating a little over 21k (16 * 4/3). So, in this test
     * we need to have a message larger than that.
     */
    certlen = i2d_X509(chaincert, NULL);
    OPENSSL_assert((certlen * NUM_EXTRA_CERTS)
                   > ((SSL3_RT_MAX_PLAIN_LENGTH * 4) / 3));
    for (i = 0; i < NUM_EXTRA_CERTS; i++) {
        if (!X509_up_ref(chaincert)) {
            printf("Unable to up ref cert\n");
            goto end;
        }
        if (!SSL_CTX_add_extra_chain_cert(sctx, chaincert)) {
            printf("Unable to add extra chain cert %d\n", i);
            X509_free(chaincert);
            goto end;
        }
    }

    if (!create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL, NULL)) {
        printf("Unable to create SSL objects\n");
        goto end;
    }

    if (!create_ssl_connection(serverssl, clientssl)) {
        printf("Unable to create SSL connection\n");
        goto end;
    }

    /*
     * Calling SSL_clear() first is not required but this tests that SSL_clear()
     * doesn't leak (when using enable-crypto-mdebug).
     */
    if (!SSL_clear(serverssl)) {
        printf("Unexpected failure from SSL_clear()\n");
        goto end;
    }

    testresult = 1;
 end:
    X509_free(chaincert);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static int test_large_message_tls(void)
{
    return execute_test_large_message(TLS_server_method(), TLS_client_method(),
                                      TLS1_VERSION, TLS_MAX_VERSION,
                                      0);
}

static int test_large_message_tls_read_ahead(void)
{
    return execute_test_large_message(TLS_server_method(), TLS_client_method(),
                                      TLS1_VERSION, TLS_MAX_VERSION,
                                      1);
}

#ifndef OPENSSL_NO_DTLS
static int test_large_message_dtls(void)
{
    /*
     * read_ahead is not relevant to DTLS because DTLS always acts as if
     * read_ahead is set.
     */
    return execute_test_large_message(DTLS_server_method(),
                                      DTLS_client_method(),
                                      DTLS1_VERSION, DTLS_MAX_VERSION,
                                      0);
}
#endif

#ifndef OPENSSL_NO_OCSP
static int ocsp_server_cb(SSL *s, void *arg)
{
    int *argi = (int *)arg;
    unsigned char *orespdercopy = NULL;
    STACK_OF(OCSP_RESPID) *ids = NULL;
    OCSP_RESPID *id = NULL;

    if (*argi == 2) {
        /* In this test we are expecting exactly 1 OCSP_RESPID */
        SSL_get_tlsext_status_ids(s, &ids);
        if (ids == NULL || sk_OCSP_RESPID_num(ids) != 1)
            return SSL_TLSEXT_ERR_ALERT_FATAL;

        id = sk_OCSP_RESPID_value(ids, 0);
        if (id == NULL || !OCSP_RESPID_match(id, ocspcert))
            return SSL_TLSEXT_ERR_ALERT_FATAL;
    } else if (*argi != 1) {
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }


    orespdercopy = OPENSSL_memdup(orespder, sizeof(orespder));
    if (orespdercopy == NULL)
        return SSL_TLSEXT_ERR_ALERT_FATAL;

    SSL_set_tlsext_status_ocsp_resp(s, orespdercopy, sizeof(orespder));

    ocsp_server_called = 1;

    return SSL_TLSEXT_ERR_OK;
}

static int ocsp_client_cb(SSL *s, void *arg)
{
    int *argi = (int *)arg;
    const unsigned char *respderin;
    size_t len;

    if (*argi != 1 && *argi != 2)
        return 0;

    len = SSL_get_tlsext_status_ocsp_resp(s, &respderin);

    if (memcmp(orespder, respderin, len) != 0)
        return 0;

    ocsp_client_called = 1;

    return 1;
}

static int test_tlsext_status_type(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    STACK_OF(OCSP_RESPID) *ids = NULL;
    OCSP_RESPID *id = NULL;
    BIO *certbio = NULL;

    if (!create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                             TLS1_VERSION, TLS_MAX_VERSION, &sctx, &cctx,
                             cert, privkey)) {
        printf("Unable to create SSL_CTX pair\n");
        return 0;
    }

    if (SSL_CTX_get_tlsext_status_type(cctx) != -1) {
        printf("Unexpected initial value for "
               "SSL_CTX_get_tlsext_status_type()\n");
        goto end;
    }

    /* First just do various checks getting and setting tlsext_status_type */

    clientssl = SSL_new(cctx);
    if (SSL_get_tlsext_status_type(clientssl) != -1) {
        printf("Unexpected initial value for SSL_get_tlsext_status_type()\n");
        goto end;
    }

    if (!SSL_set_tlsext_status_type(clientssl, TLSEXT_STATUSTYPE_ocsp)) {
        printf("Unexpected fail for SSL_set_tlsext_status_type()\n");
        goto end;
    }

    if (SSL_get_tlsext_status_type(clientssl) != TLSEXT_STATUSTYPE_ocsp) {
        printf("Unexpected result for SSL_get_tlsext_status_type()\n");
        goto end;
    }

    SSL_free(clientssl);
    clientssl = NULL;

    if (!SSL_CTX_set_tlsext_status_type(cctx, TLSEXT_STATUSTYPE_ocsp)) {
        printf("Unexpected fail for SSL_CTX_set_tlsext_status_type()\n");
        goto end;
    }

    if (SSL_CTX_get_tlsext_status_type(cctx) != TLSEXT_STATUSTYPE_ocsp) {
        printf("Unexpected result for SSL_CTX_get_tlsext_status_type()\n");
        goto end;
    }

    clientssl = SSL_new(cctx);

    if (SSL_get_tlsext_status_type(clientssl) != TLSEXT_STATUSTYPE_ocsp) {
        printf("Unexpected result for SSL_get_tlsext_status_type() (test 2)\n");
        goto end;
    }

    SSL_free(clientssl);
    clientssl = NULL;

    /*
     * Now actually do a handshake and check OCSP information is exchanged and
     * the callbacks get called
     */

    SSL_CTX_set_tlsext_status_cb(cctx, ocsp_client_cb);
    SSL_CTX_set_tlsext_status_arg(cctx, &cdummyarg);
    SSL_CTX_set_tlsext_status_cb(sctx, ocsp_server_cb);
    SSL_CTX_set_tlsext_status_arg(sctx, &cdummyarg);

    if (!create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL, NULL)) {
        printf("Unable to create SSL objects\n");
        goto end;
    }

    if (!create_ssl_connection(serverssl, clientssl)) {
        printf("Unable to create SSL connection\n");
        goto end;
    }

    if (!ocsp_client_called || !ocsp_server_called) {
        printf("OCSP callbacks not called\n");
        goto end;
    }

    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = NULL;
    clientssl = NULL;

    /* Try again but this time force the server side callback to fail */
    ocsp_client_called = 0;
    ocsp_server_called = 0;
    cdummyarg = 0;

    if (!create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL, NULL)) {
        printf("Unable to create SSL objects\n");
        goto end;
    }

    /* This should fail because the callback will fail */
    if (create_ssl_connection(serverssl, clientssl)) {
        printf("Unexpected success creating the connection\n");
        goto end;
    }

    if (ocsp_client_called || ocsp_server_called) {
        printf("OCSP callbacks successfully called unexpectedly\n");
        goto end;
    }

    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = NULL;
    clientssl = NULL;

    /*
     * This time we'll get the client to send an OCSP_RESPID that it will
     * accept.
     */
    ocsp_client_called = 0;
    ocsp_server_called = 0;
    cdummyarg = 2;

    if (!create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL, NULL)) {
        printf("Unable to create SSL objects\n");
        goto end;
    }

    /*
     * We'll just use any old cert for this test - it doesn't have to be an OCSP
     * specific one. We'll use the server cert.
     */
    certbio = BIO_new_file(cert, "r");
    if (certbio == NULL) {
        printf("Can't load the certificate file\n");
        goto end;
    }
    id = OCSP_RESPID_new();
    ids = sk_OCSP_RESPID_new_null();
    ocspcert = PEM_read_bio_X509(certbio, NULL, NULL, NULL);
    if (id == NULL || ids == NULL || ocspcert == NULL
            || !OCSP_RESPID_set_by_key(id, ocspcert)
            || !sk_OCSP_RESPID_push(ids, id)) {
        printf("Unable to set OCSP_RESPIDs\n");
        goto end;
    }
    id = NULL;
    SSL_set_tlsext_status_ids(clientssl, ids);
    /* Control has been transferred */
    ids = NULL;

    BIO_free(certbio);
    certbio = NULL;

    if (!create_ssl_connection(serverssl, clientssl)) {
        printf("Unable to create SSL connection\n");
        goto end;
    }

    if (!ocsp_client_called || !ocsp_server_called) {
        printf("OCSP callbacks not called\n");
        goto end;
    }

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    sk_OCSP_RESPID_pop_free(ids, OCSP_RESPID_free);
    OCSP_RESPID_free(id);
    BIO_free(certbio);
    X509_free(ocspcert);
    ocspcert = NULL;

    return testresult;
}
#endif  /* ndef OPENSSL_NO_OCSP */

typedef struct ssl_session_test_fixture {
    const char *test_case_name;
    int use_ext_cache;
    int use_int_cache;
} SSL_SESSION_TEST_FIXTURE;

static int new_called = 0, remove_called = 0;

static SSL_SESSION_TEST_FIXTURE
ssl_session_set_up(const char *const test_case_name)
{
    SSL_SESSION_TEST_FIXTURE fixture;

    fixture.test_case_name = test_case_name;
    fixture.use_ext_cache = 1;
    fixture.use_int_cache = 1;

    new_called = remove_called = 0;

    return fixture;
}

static void ssl_session_tear_down(SSL_SESSION_TEST_FIXTURE fixture)
{
}

static int new_session_cb(SSL *ssl, SSL_SESSION *sess)
{
    new_called++;

    return 1;
}

static void remove_session_cb(SSL_CTX *ctx, SSL_SESSION *sess)
{
    remove_called++;
}

static int execute_test_session(SSL_SESSION_TEST_FIXTURE fix)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *serverssl1 = NULL, *clientssl1 = NULL;
    SSL *serverssl2 = NULL, *clientssl2 = NULL;
#ifndef OPENSSL_NO_TLS1_1
    SSL *serverssl3 = NULL, *clientssl3 = NULL;
#endif
    SSL_SESSION *sess1 = NULL, *sess2 = NULL;
    int testresult = 0;

    if (!create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                             TLS1_VERSION, TLS_MAX_VERSION, &sctx, &cctx,
                             cert, privkey)) {
        printf("Unable to create SSL_CTX pair\n");
        return 0;
    }

#ifndef OPENSSL_NO_TLS1_2
    /* Only allow TLS1.2 so we can force a connection failure later */
    SSL_CTX_set_min_proto_version(cctx, TLS1_2_VERSION);
#endif

    /* Set up session cache */
    if (fix.use_ext_cache) {
        SSL_CTX_sess_set_new_cb(cctx, new_session_cb);
        SSL_CTX_sess_set_remove_cb(cctx, remove_session_cb);
    }
    if (fix.use_int_cache) {
        /* Also covers instance where both are set */
        SSL_CTX_set_session_cache_mode(cctx, SSL_SESS_CACHE_CLIENT);
    } else {
        SSL_CTX_set_session_cache_mode(cctx,
                                       SSL_SESS_CACHE_CLIENT
                                       | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    }

    if (!create_ssl_objects(sctx, cctx, &serverssl1, &clientssl1, NULL,
                               NULL)) {
        printf("Unable to create SSL objects\n");
        goto end;
    }

    if (!create_ssl_connection(serverssl1, clientssl1)) {
        printf("Unable to create SSL connection\n");
        goto end;
    }
    sess1 = SSL_get1_session(clientssl1);
    if (sess1 == NULL) {
        printf("Unexpected NULL session\n");
        goto end;
    }

    if (fix.use_int_cache && SSL_CTX_add_session(cctx, sess1)) {
        /* Should have failed because it should already be in the cache */
        printf("Unexpected success adding session to cache\n");
        goto end;
    }

    if (fix.use_ext_cache && (new_called != 1 || remove_called != 0)) {
        printf("Session not added to cache\n");
        goto end;
    }

    if (!create_ssl_objects(sctx, cctx, &serverssl2, &clientssl2, NULL, NULL)) {
        printf("Unable to create second SSL objects\n");
        goto end;
    }

    if (!create_ssl_connection(serverssl2, clientssl2)) {
        printf("Unable to create second SSL connection\n");
        goto end;
    }

    sess2 = SSL_get1_session(clientssl2);
    if (sess2 == NULL) {
        printf("Unexpected NULL session from clientssl2\n");
        goto end;
    }

    if (fix.use_ext_cache && (new_called != 2 || remove_called != 0)) {
        printf("Remove session callback unexpectedly called\n");
        goto end;
    }

    /*
     * This should clear sess2 from the cache because it is a "bad" session. See
     * SSL_set_session() documentation.
     */
    if (!SSL_set_session(clientssl2, sess1)) {
        printf("Unexpected failure setting session\n");
        goto end;
    }

    if (fix.use_ext_cache && (new_called != 2 || remove_called != 1)) {
        printf("Failed to call callback to remove session\n");
        goto end;
    }


    if (SSL_get_session(clientssl2) != sess1) {
        printf("Unexpected session found\n");
        goto end;
    }

    if (fix.use_int_cache) {
        if (!SSL_CTX_add_session(cctx, sess2)) {
            /*
             * Should have succeeded because it should not already be in the cache
             */
            printf("Unexpected failure adding session to cache\n");
            goto end;
        }

        if (!SSL_CTX_remove_session(cctx, sess2)) {
            printf("Unexpected failure removing session from cache\n");
            goto end;
        }

        /* This is for the purposes of internal cache testing...ignore the
         * counter for external cache
         */
        if (fix.use_ext_cache)
            remove_called--;
    }

    /* This shouldn't be in the cache so should fail */
    if (SSL_CTX_remove_session(cctx, sess2)) {
        printf("Unexpected success removing session from cache\n");
        goto end;
    }

    if (fix.use_ext_cache && (new_called != 2 || remove_called != 2)) {
        printf("Failed to call callback to remove session #2\n");
        goto end;
    }

#if !defined(OPENSSL_NO_TLS1_1) && !defined(OPENSSL_NO_TLS1_2)
    /* Force a connection failure */
    SSL_CTX_set_max_proto_version(sctx, TLS1_1_VERSION);

    if (!create_ssl_objects(sctx, cctx, &serverssl3, &clientssl3, NULL, NULL)) {
        printf("Unable to create third SSL objects\n");
        goto end;
    }

    if (!SSL_set_session(clientssl3, sess1)) {
        printf("Unable to set session for third connection\n");
        goto end;
    }

    /* This should fail because of the mismatched protocol versions */
    if (create_ssl_connection(serverssl3, clientssl3)) {
        printf("Unable to create third SSL connection\n");
        goto end;
    }


    /* We should have automatically removed the session from the cache */
    if (fix.use_ext_cache && (new_called != 2 || remove_called != 3)) {
        printf("Failed to call callback to remove session #2\n");
        goto end;
    }

    if (fix.use_int_cache && !SSL_CTX_add_session(cctx, sess2)) {
        /*
         * Should have succeeded because it should not already be in the cache
         */
        printf("Unexpected failure adding session to cache #2\n");
        goto end;
    }
#endif

    testresult = 1;

 end:
    SSL_free(serverssl1);
    SSL_free(clientssl1);
    SSL_free(serverssl2);
    SSL_free(clientssl2);
#ifndef OPENSSL_NO_TLS1_1
    SSL_free(serverssl3);
    SSL_free(clientssl3);
#endif
    SSL_SESSION_free(sess1);
    SSL_SESSION_free(sess2);
    /*
     * Check if we need to remove any sessions up-refed for the external cache
     */
    if (new_called >= 1)
        SSL_SESSION_free(sess1);
    if (new_called >= 2)
        SSL_SESSION_free(sess2);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static int test_session_with_only_int_cache(void)
{
    SETUP_TEST_FIXTURE(SSL_SESSION_TEST_FIXTURE, ssl_session_set_up);

    fixture.use_ext_cache = 0;

    EXECUTE_TEST(execute_test_session, ssl_session_tear_down);
}

static int test_session_with_only_ext_cache(void)
{
    SETUP_TEST_FIXTURE(SSL_SESSION_TEST_FIXTURE, ssl_session_set_up);

    fixture.use_int_cache = 0;

    EXECUTE_TEST(execute_test_session, ssl_session_tear_down);
}

static int test_session_with_both_cache(void)
{
    SETUP_TEST_FIXTURE(SSL_SESSION_TEST_FIXTURE, ssl_session_set_up);

    EXECUTE_TEST(execute_test_session, ssl_session_tear_down);
}

#define USE_NULL    0
#define USE_BIO_1   1
#define USE_BIO_2   2

#define TOTAL_SSL_SET_BIO_TESTS (3 * 3 * 3 * 3)

static void setupbio(BIO **res, BIO *bio1, BIO *bio2, int type)
{
    switch (type) {
    case USE_NULL:
        *res = NULL;
        break;
    case USE_BIO_1:
        *res = bio1;
        break;
    case USE_BIO_2:
        *res = bio2;
        break;
    }
}

static int test_ssl_set_bio(int idx)
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_method());
    BIO *bio1 = NULL;
    BIO *bio2 = NULL;
    BIO *irbio = NULL, *iwbio = NULL, *nrbio = NULL, *nwbio = NULL;
    SSL *ssl = NULL;
    int initrbio, initwbio, newrbio, newwbio;
    int testresult = 0;

    if (ctx == NULL) {
        printf("Failed to allocate SSL_CTX\n");
        goto end;
    }

    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        printf("Failed to allocate SSL object\n");
        goto end;
    }

    initrbio = idx % 3;
    idx /= 3;
    initwbio = idx % 3;
    idx /= 3;
    newrbio = idx % 3;
    idx /= 3;
    newwbio = idx;
    OPENSSL_assert(newwbio <= 2);

    if (initrbio == USE_BIO_1 || initwbio == USE_BIO_1 || newrbio == USE_BIO_1
            || newwbio == USE_BIO_1) {
        bio1 = BIO_new(BIO_s_mem());
        if (bio1 == NULL) {
            printf("Failed to allocate bio1\n");
            goto end;
        }
    }

    if (initrbio == USE_BIO_2 || initwbio == USE_BIO_2 || newrbio == USE_BIO_2
            || newwbio == USE_BIO_2) {
        bio2 = BIO_new(BIO_s_mem());
        if (bio2 == NULL) {
            printf("Failed to allocate bio2\n");
            goto end;
        }
    }

    setupbio(&irbio, bio1, bio2, initrbio);
    setupbio(&iwbio, bio1, bio2, initwbio);

    /*
     * We want to maintain our own refs to these BIO, so do an up ref for each
     * BIO that will have ownership transferred in the SSL_set_bio() call
     */
    if (irbio != NULL)
        BIO_up_ref(irbio);
    if (iwbio != NULL && iwbio != irbio)
        BIO_up_ref(iwbio);

    SSL_set_bio(ssl, irbio, iwbio);

    setupbio(&nrbio, bio1, bio2, newrbio);
    setupbio(&nwbio, bio1, bio2, newwbio);

    /*
     * We will (maybe) transfer ownership again so do more up refs.
     * SSL_set_bio() has some really complicated ownership rules where BIOs have
     * already been set!
     */
    if (nrbio != NULL && nrbio != irbio && (nwbio != iwbio || nrbio != nwbio))
        BIO_up_ref(nrbio);
    if (nwbio != NULL && nwbio != nrbio && (nwbio != iwbio || (nwbio == iwbio && irbio == iwbio)))
        BIO_up_ref(nwbio);

    SSL_set_bio(ssl, nrbio, nwbio);

    testresult = 1;

 end:
    SSL_free(ssl);
    BIO_free(bio1);
    BIO_free(bio2);
    /*
     * This test is checking that the ref counting for SSL_set_bio is correct.
     * If we get here and we did too many frees then we will fail in the above
     * functions. If we haven't done enough then this will only be detected in
     * a crypto-mdebug build
     */
    SSL_CTX_free(ctx);

    return testresult;
}

typedef struct ssl_bio_test_fixture {
    const char *test_case_name;
    int pop_ssl;
    enum { NO_BIO_CHANGE, CHANGE_RBIO, CHANGE_WBIO } change_bio;
} SSL_BIO_TEST_FIXTURE;

static SSL_BIO_TEST_FIXTURE ssl_bio_set_up(const char *const test_case_name)
{
    SSL_BIO_TEST_FIXTURE fixture;

    fixture.test_case_name = test_case_name;
    fixture.pop_ssl = 0;
    fixture.change_bio = NO_BIO_CHANGE;

    return fixture;
}

static void ssl_bio_tear_down(SSL_BIO_TEST_FIXTURE fixture)
{
}

static int execute_test_ssl_bio(SSL_BIO_TEST_FIXTURE fix)
{
    BIO *sslbio = NULL, *membio1 = NULL, *membio2 = NULL;
    SSL_CTX *ctx = SSL_CTX_new(TLS_method());
    SSL *ssl = NULL;
    int testresult = 0;

    if (ctx == NULL) {
        printf("Failed to allocate SSL_CTX\n");
        return 0;
    }

    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        printf("Failed to allocate SSL object\n");
        goto end;
    }

    sslbio = BIO_new(BIO_f_ssl());
    membio1 = BIO_new(BIO_s_mem());

    if (sslbio == NULL || membio1 == NULL) {
        printf("Malloc failure creating BIOs\n");
        goto end;
    }

    BIO_set_ssl(sslbio, ssl, BIO_CLOSE);

    /*
     * If anything goes wrong here then we could leak memory, so this will
     * be caught in a crypto-mdebug build
     */
    BIO_push(sslbio, membio1);

    /* Verify changing the rbio/wbio directly does not cause leaks */
    if (fix.change_bio != NO_BIO_CHANGE) {
        membio2 = BIO_new(BIO_s_mem());
        if (membio2 == NULL) {
            printf("Malloc failure creating membio2\n");
            goto end;
        }
        if (fix.change_bio == CHANGE_RBIO)
            SSL_set0_rbio(ssl, membio2);
        else
            SSL_set0_wbio(ssl, membio2);
    }
    ssl = NULL;

    if (fix.pop_ssl)
        BIO_pop(sslbio);
    else
        BIO_pop(membio1);

    testresult = 1;
 end:
    BIO_free(membio1);
    BIO_free(sslbio);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return testresult;
}

static int test_ssl_bio_pop_next_bio(void)
{
    SETUP_TEST_FIXTURE(SSL_BIO_TEST_FIXTURE, ssl_bio_set_up);

    EXECUTE_TEST(execute_test_ssl_bio, ssl_bio_tear_down);
}

static int test_ssl_bio_pop_ssl_bio(void)
{
    SETUP_TEST_FIXTURE(SSL_BIO_TEST_FIXTURE, ssl_bio_set_up);

    fixture.pop_ssl = 1;

    EXECUTE_TEST(execute_test_ssl_bio, ssl_bio_tear_down);
}

static int test_ssl_bio_change_rbio(void)
{
    SETUP_TEST_FIXTURE(SSL_BIO_TEST_FIXTURE, ssl_bio_set_up);

    fixture.change_bio = CHANGE_RBIO;

    EXECUTE_TEST(execute_test_ssl_bio, ssl_bio_tear_down);
}

static int test_ssl_bio_change_wbio(void)
{
    SETUP_TEST_FIXTURE(SSL_BIO_TEST_FIXTURE, ssl_bio_set_up);

    fixture.change_bio = CHANGE_WBIO;

    EXECUTE_TEST(execute_test_ssl_bio, ssl_bio_tear_down);
}

typedef struct {
    /* The list of sig algs */
    const int *list;
    /* The length of the list */
    size_t listlen;
    /* A sigalgs list in string format */
    const char *liststr;
    /* Whether setting the list should succeed */
    int valid;
    /* Whether creating a connection with the list should succeed */
    int connsuccess;
} sigalgs_list;

static const int validlist1[] = {NID_sha256, EVP_PKEY_RSA};
static const int validlist2[] = {NID_sha256, EVP_PKEY_RSA, NID_sha512, EVP_PKEY_EC};
static const int validlist3[] = {NID_sha512, EVP_PKEY_EC};
static const int invalidlist1[] = {NID_undef, EVP_PKEY_RSA};
static const int invalidlist2[] = {NID_sha256, NID_undef};
static const int invalidlist3[] = {NID_sha256, EVP_PKEY_RSA, NID_sha256};
static const int invalidlist4[] = {NID_sha256};
static const sigalgs_list testsigalgs[] = {
    {validlist1, OSSL_NELEM(validlist1), NULL, 1, 1},
    {validlist2, OSSL_NELEM(validlist2), NULL, 1, 1},
    {validlist3, OSSL_NELEM(validlist3), NULL, 1, 0},
    {NULL, 0, "RSA+SHA256", 1, 1},
    {NULL, 0, "RSA+SHA256:ECDSA+SHA512", 1, 1},
    {NULL, 0, "ECDSA+SHA512", 1, 0},
    {invalidlist1, OSSL_NELEM(invalidlist1), NULL, 0, 0},
    {invalidlist2, OSSL_NELEM(invalidlist2), NULL, 0, 0},
    {invalidlist3, OSSL_NELEM(invalidlist3), NULL, 0, 0},
    {invalidlist4, OSSL_NELEM(invalidlist4), NULL, 0, 0},
    {NULL, 0, "RSA", 0, 0},
    {NULL, 0, "SHA256", 0, 0},
    {NULL, 0, "RSA+SHA256:SHA256", 0, 0},
    {NULL, 0, "Invalid", 0, 0}};

static int test_set_sigalgs(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    const sigalgs_list *curr;
    int testctx;

    /* Should never happen */
    if ((size_t)idx >= OSSL_NELEM(testsigalgs) * 2)
        return 0;

    testctx = ((size_t)idx < OSSL_NELEM(testsigalgs));
    curr = testctx ? &testsigalgs[idx]
                   : &testsigalgs[idx - OSSL_NELEM(testsigalgs)];

    if (!create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                             TLS1_VERSION, TLS_MAX_VERSION, &sctx, &cctx,
                             cert, privkey)) {
        printf("Unable to create SSL_CTX pair\n");
        return 0;
    }

    if (testctx) {
        int ret;
        if (curr->list != NULL)
            ret = SSL_CTX_set1_sigalgs(cctx, curr->list, curr->listlen);
        else
            ret = SSL_CTX_set1_sigalgs_list(cctx, curr->liststr);

        if (!ret) {
            if (curr->valid)
                printf("Unexpected failure setting sigalgs in SSL_CTX (%d)\n",
                       idx);
            else
                testresult = 1;
            goto end;
        }
        if (!curr->valid) {
            printf("Unexpected success setting sigalgs in SSL_CTX (%d)\n", idx);
            goto end;
        }
    }

    if (!create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL, NULL)) {
        printf("Unable to create SSL objects\n");
        goto end;
    }

    if (!testctx) {
        int ret;

        if (curr->list != NULL)
            ret = SSL_set1_sigalgs(clientssl, curr->list, curr->listlen);
        else
            ret = SSL_set1_sigalgs_list(clientssl, curr->liststr);
        if (!ret) {
            if (curr->valid)
                printf("Unexpected failure setting sigalgs in SSL (%d)\n", idx);
            else
                testresult = 1;
            goto end;
        }
        if (!curr->valid) {
            printf("Unexpected success setting sigalgs in SSL (%d)\n", idx);
            goto end;
        }
    }

    if (curr->connsuccess != create_ssl_connection(serverssl, clientssl)) {
        printf("Unexpected return value creating SSL connection (%d)\n", idx);
        goto end;
    }

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static int clntaddcb = 0;
static int clntparsecb = 0;
static int srvaddcb = 0;
static int srvparsecb = 0;
static int snicb = 0;

#define TEST_EXT_TYPE1  0xff00

static int add_cb(SSL *s, unsigned int ext_type, const unsigned char **out,
                  size_t *outlen, int *al, void *add_arg)
{
    int *server = (int *)add_arg;
    unsigned char *data;

    if (SSL_is_server(s))
        srvaddcb++;
    else
        clntaddcb++;

    if (*server != SSL_is_server(s)
            || (data = OPENSSL_malloc(sizeof(*data))) == NULL)
        return -1;

    *data = 1;
    *out = data;
    *outlen = sizeof(char);
    return 1;
}

static void free_cb(SSL *s, unsigned int ext_type, const unsigned char *out,
                    void *add_arg)
{
    OPENSSL_free((unsigned char *)out);
}

static int parse_cb(SSL *s, unsigned int ext_type, const unsigned char *in,
                    size_t inlen, int *al, void *parse_arg)
{
    int *server = (int *)parse_arg;

    if (SSL_is_server(s))
        srvparsecb++;
    else
        clntparsecb++;

    if (*server != SSL_is_server(s)
            || inlen != sizeof(char)
            || *in != 1)
        return -1;

    return 1;
}

static int sni_cb(SSL *s, int *al, void *arg)
{
    SSL_CTX *ctx = (SSL_CTX *)arg;

    if (SSL_set_SSL_CTX(s, ctx) == NULL) {
        *al = SSL_AD_INTERNAL_ERROR;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }
    snicb++;
    return SSL_TLSEXT_ERR_OK;
}

/*
 * Custom call back tests.
 * Test 0: callbacks in TLSv1.2
 * Test 1: callbacks in TLSv1.2 with SNI
 */
static int test_custom_exts(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL, *sctx2 = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    static int server = 1;
    static int client = 0;
    SSL_SESSION *sess = NULL;

    /* Reset callback counters */
    clntaddcb = clntparsecb = srvaddcb = srvparsecb = 0;
    snicb = 0;

    if (!create_ssl_ctx_pair(TLS_server_method(),  TLS_client_method(),
                             TLS1_VERSION, TLS_MAX_VERSION, &sctx, &cctx,
                             cert, privkey)) {
        printf("Unable to create SSL_CTX pair\n");
        goto end;
    }

    if (tst == 1
            && !create_ssl_ctx_pair(TLS_server_method(), NULL,
                                    TLS1_VERSION, TLS_MAX_VERSION, &sctx2, NULL,
                                    cert, privkey)) {
        printf("Unable to create SSL_CTX pair (2)\n");
        goto end;
    }

    /* Create a client side custom extension */
    if (!SSL_CTX_add_client_custom_ext(cctx, TEST_EXT_TYPE1, add_cb, free_cb,
                                       &client, parse_cb, &client)) {
        printf("Unable to add client custom extension\n");
        goto end;
    }

    /* Should not be able to add duplicates */
    if (SSL_CTX_add_client_custom_ext(cctx, TEST_EXT_TYPE1, add_cb, free_cb,
                                      &client, parse_cb, &client)) {
        printf("Unexpected success adding duplicate extension\n");
        goto end;
    }

    /* Create a server side custom extension */
    if (!SSL_CTX_add_server_custom_ext(sctx, TEST_EXT_TYPE1, add_cb, free_cb,
                                       &server, parse_cb, &server)) {
        printf("Unable to add server custom extension\n");
        goto end;
    }
    if (sctx2 != NULL
            && !SSL_CTX_add_server_custom_ext(sctx2, TEST_EXT_TYPE1,
                                                        add_cb, free_cb,
                                                        &server, parse_cb,
                                                        &server)) {
        printf("Unable to add server custom extension for SNI\n");
        goto end;
    }

    /* Should not be able to add duplicates */
    if (SSL_CTX_add_server_custom_ext(sctx, TEST_EXT_TYPE1, add_cb, free_cb,
                                      &server, parse_cb, &server)) {
        printf("Unexpected success adding duplicate extension (2)\n");
        goto end;
    }

    if (tst == 1) {
        /* Set up SNI */
        if (!SSL_CTX_set_tlsext_servername_callback(sctx, sni_cb)
                || !SSL_CTX_set_tlsext_servername_arg(sctx, sctx2)) {
            printf("Cannot set SNI callbacks\n");
            goto end;
        }
    }

    if (!create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL, NULL)
            || !create_ssl_connection(serverssl, clientssl)) {
        printf("Cannot create SSL connection\n");
        goto end;
    }

    if (clntaddcb != 1
            || clntparsecb != 1
            || srvaddcb != 1
            || srvparsecb != 1
            || (tst != 1 && snicb != 0)
            || (tst == 1 && snicb != 1)) {
        printf("Incorrect callback counts\n");
        goto end;
    }

    sess = SSL_get1_session(clientssl);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    if (tst == 1) {
        /* We don't bother with the resumption aspects for this test */
        testresult = 1;
        goto end;
    }

    if (!create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL, NULL)
            || !SSL_set_session(clientssl, sess)
            || !create_ssl_connection(serverssl, clientssl)) {
        printf("Cannot create resumption connection\n");
        goto end;
    }

    /*
     * For a resumed session we expect to add the ClientHello extension but we
     * should ignore it on the server side.
     */
    if (clntaddcb != 2
            || clntparsecb != 1
            || srvaddcb != 1
            || srvparsecb != 1) {
        printf("Incorrect resumption callback counts\n");
        goto end;
    }

    testresult = 1;

end:
    SSL_SESSION_free(sess);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx2);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

int main(int argc, char *argv[])
{
    BIO *err = NULL;
    int testresult = 1;

    if (argc != 3) {
        printf("Invalid argument count\n");
        return 1;
    }

    cert = argv[1];
    privkey = argv[2];

    err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    ADD_TEST(test_large_message_tls);
    ADD_TEST(test_large_message_tls_read_ahead);
#ifndef OPENSSL_NO_DTLS
    ADD_TEST(test_large_message_dtls);
#endif
#ifndef OPENSSL_NO_OCSP
    ADD_TEST(test_tlsext_status_type);
#endif
    ADD_TEST(test_session_with_only_int_cache);
    ADD_TEST(test_session_with_only_ext_cache);
    ADD_TEST(test_session_with_both_cache);
    ADD_ALL_TESTS(test_ssl_set_bio, TOTAL_SSL_SET_BIO_TESTS);
    ADD_TEST(test_ssl_bio_pop_next_bio);
    ADD_TEST(test_ssl_bio_pop_ssl_bio);
    ADD_TEST(test_ssl_bio_change_rbio);
    ADD_TEST(test_ssl_bio_change_wbio);
    ADD_ALL_TESTS(test_set_sigalgs, OSSL_NELEM(testsigalgs) * 2);
    ADD_ALL_TESTS(test_custom_exts, 2);

    testresult = run_tests(argv[0]);

    bio_s_mempacket_test_free();

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(err) <= 0)
        testresult = 1;
#endif
    BIO_free(err);

    if (!testresult)
        printf("PASS\n");

    return testresult;
}
