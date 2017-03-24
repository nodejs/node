/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "ssltestlib.h"
#include "testutil.h"

static void copy_flags(BIO *bio)
{
    int flags;
    BIO *next = BIO_next(bio);

    flags = BIO_test_flags(next, BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_RWS);
    BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_RWS);
    BIO_set_flags(bio, flags);
}

static int tls_corrupt_read(BIO *bio, char *out, int outl)
{
    int ret;
    BIO *next = BIO_next(bio);

    ret = BIO_read(next, out, outl);
    copy_flags(bio);

    return ret;
}

static int tls_corrupt_write(BIO *bio, const char *in, int inl)
{
    int ret;
    BIO *next = BIO_next(bio);
    char *copy;

    if (in[0] == SSL3_RT_APPLICATION_DATA) {
        copy = BUF_memdup(in, inl);
        TEST_check(copy != NULL);
        /* corrupt last bit of application data */
        copy[inl-1] ^= 1;
        ret = BIO_write(next, copy, inl);
        OPENSSL_free(copy);
    } else {
        ret = BIO_write(next, in, inl);
    }
    copy_flags(bio);

    return ret;
}

static long tls_corrupt_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
    long ret;
    BIO *next = BIO_next(bio);

    if (next == NULL)
        return 0;

    switch (cmd) {
    case BIO_CTRL_DUP:
        ret = 0L;
        break;
    default:
        ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    }
    return ret;
}

static int tls_corrupt_gets(BIO *bio, char *buf, int size)
{
    /* We don't support this - not needed anyway */
    return -1;
}

static int tls_corrupt_puts(BIO *bio, const char *str)
{
    /* We don't support this - not needed anyway */
    return -1;
}

static int tls_corrupt_new(BIO *bio)
{
    BIO_set_init(bio, 1);

    return 1;
}

static int tls_corrupt_free(BIO *bio)
{
    BIO_set_init(bio, 0);

    return 1;
}

#define BIO_TYPE_CUSTOM_FILTER  (0x80 | BIO_TYPE_FILTER)

static BIO_METHOD *method_tls_corrupt = NULL;

/* Note: Not thread safe! */
static const BIO_METHOD *bio_f_tls_corrupt_filter(void)
{
    if (method_tls_corrupt == NULL) {
        method_tls_corrupt = BIO_meth_new(BIO_TYPE_CUSTOM_FILTER,
                                          "TLS corrupt filter");
        if (   method_tls_corrupt == NULL
            || !BIO_meth_set_write(method_tls_corrupt, tls_corrupt_write)
            || !BIO_meth_set_read(method_tls_corrupt, tls_corrupt_read)
            || !BIO_meth_set_puts(method_tls_corrupt, tls_corrupt_puts)
            || !BIO_meth_set_gets(method_tls_corrupt, tls_corrupt_gets)
            || !BIO_meth_set_ctrl(method_tls_corrupt, tls_corrupt_ctrl)
            || !BIO_meth_set_create(method_tls_corrupt, tls_corrupt_new)
            || !BIO_meth_set_destroy(method_tls_corrupt, tls_corrupt_free))
            return NULL;
    }
    return method_tls_corrupt;
}

static void bio_f_tls_corrupt_filter_free(void)
{
    BIO_meth_free(method_tls_corrupt);
}

/*
 * The test is supposed to be executed with RSA key, customarily
 * with apps/server.pem used even in other tests. For this reason
 * |cipher_list| is initialized with RSA ciphers' names. This
 * naturally means that if test is to be re-purposed for other
 * type of key, then NID_auth_* filter below would need adjustment.
 */
static const char **cipher_list = NULL;

static int setup_cipher_list()
{
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    static STACK_OF(SSL_CIPHER) *sk_ciphers = NULL;
    int i, numciphers;

    ctx = SSL_CTX_new(TLS_server_method());
    TEST_check(ctx != NULL);
    ssl = SSL_new(ctx);
    TEST_check(ssl != NULL);
    sk_ciphers = SSL_get1_supported_ciphers(ssl);
    TEST_check(sk_ciphers != NULL);

    /*
     * The |cipher_list| will be filled only with names of RSA ciphers,
     * so that some of the allocated space will be wasted, but the loss
     * is deemed acceptable...
     */
    cipher_list = OPENSSL_malloc(sk_SSL_CIPHER_num(sk_ciphers) *
                                 sizeof(cipher_list[0]));
    TEST_check(cipher_list != NULL);

    for (numciphers = 0, i = 0; i < sk_SSL_CIPHER_num(sk_ciphers); i++) {
        const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(sk_ciphers, i);

        if (SSL_CIPHER_get_auth_nid(cipher) == NID_auth_rsa)
            cipher_list[numciphers++] = SSL_CIPHER_get_name(cipher);
    }
    TEST_check(numciphers != 0);

    sk_SSL_CIPHER_free(sk_ciphers);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return numciphers;
}

static char *cert = NULL;
static char *privkey = NULL;

static int test_ssl_corrupt(int testidx)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *server = NULL, *client = NULL;
    BIO *c_to_s_fbio;
    int testresult = 0;
    static unsigned char junk[16000] = { 0 };

    printf("Starting Test %d, %s\n", testidx, cipher_list[testidx]);

    if (!create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(), &sctx,
                             &cctx, cert, privkey)) {
        printf("Unable to create SSL_CTX pair\n");
        return 0;
    }

    if (!SSL_CTX_set_cipher_list(cctx, cipher_list[testidx])) {
        printf("Failed setting cipher list\n");
        goto end;
    }

    c_to_s_fbio = BIO_new(bio_f_tls_corrupt_filter());
    if (c_to_s_fbio == NULL) {
        printf("Failed to create filter BIO\n");
        goto end;
    }

    /* BIO is freed by create_ssl_connection on error */
    if (!create_ssl_objects(sctx, cctx, &server, &client, NULL,
                            c_to_s_fbio)) {
        printf("Unable to create SSL objects\n");
        ERR_print_errors_fp(stdout);
        goto end;
    }

    if (!create_ssl_connection(server, client)) {
        printf("Unable to create SSL connection\n");
        ERR_print_errors_fp(stdout);
        goto end;
    }

    if (SSL_write(client, junk, sizeof(junk)) < 0) {
        printf("Unable to SSL_write\n");
        ERR_print_errors_fp(stdout);
        goto end;
    }

    if (SSL_read(server, junk, sizeof(junk)) >= 0) {
        printf("Read should have failed with \"bad record mac\"\n");
        goto end;
    }

    if (ERR_GET_REASON(ERR_peek_error()) !=
        SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC) {
        ERR_print_errors_fp(stdout);
        goto end;
    }

    testresult = 1;
 end:
    SSL_free(server);
    SSL_free(client);
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

    ADD_ALL_TESTS(test_ssl_corrupt, setup_cipher_list());

    testresult = run_tests(argv[0]);

    bio_f_tls_corrupt_filter_free();

    OPENSSL_free(cipher_list);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(err) <= 0)
        testresult = 1;
#endif
    BIO_free(err);

    if (!testresult)
        printf("PASS\n");

    return testresult;
}
