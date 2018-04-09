/*
 * Copyright 2017-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssltestlib.h"
#include "testutil.h"
#include <string.h>

static char *cert = NULL;
static char *privkey = NULL;

static int test_fatalerr(void)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *sssl = NULL, *cssl = NULL;
    const char *msg = "Dummy";
    BIO *wbio = NULL;
    int ret = 0, len;
    char buf[80];
    unsigned char dummyrec[] = {
        0x17, 0x03, 0x03, 0x00, 0x05, 'D', 'u', 'm', 'm', 'y'
    };

    if (!create_ssl_ctx_pair(SSLv23_method(), SSLv23_method(),
                             SSL3_VERSION, TLS_MAX_VERSION, &sctx, &cctx,
                             cert, privkey)) {
        printf("Failed to create SSL_CTX pair\n");
        goto err;
    }

    /*
     * Deliberately set the cipher lists for client and server to be different
     * to force a handshake failure.
     */
    if (!SSL_CTX_set_cipher_list(sctx, "AES128-SHA")
            || !SSL_CTX_set_cipher_list(cctx, "AES256-SHA")) {
        printf("Failed to set cipher lists\n");
        goto err;
    }

    if (!create_ssl_objects(sctx, cctx, &sssl, &cssl, NULL, NULL)) {
        printf("Failed to create SSL objectx\n");
        goto err;
    }

    wbio = SSL_get_wbio(cssl);
    if (wbio == NULL) {
        printf("Unexpected NULL bio received\n");
        goto err;
    }

    if (create_ssl_connection(sssl, cssl)) {
        printf("Unexpected success creating a connection\n");
        goto err;
    }

    ERR_clear_error();

    /* Inject a plaintext record from client to server */
    if (BIO_write(wbio, dummyrec, sizeof(dummyrec)) <= 0) {
        printf("Unexpected failure injecting dummy record\n");
        goto err;
    }

    /* SSL_read()/SSL_write should fail because of a previous fatal error */
    if ((len = SSL_read(sssl, buf, sizeof(buf) - 1)) > 0) {
        buf[len] = '\0';
        printf("Unexpected success reading data: %s\n", buf);
        goto err;
    }
    if (SSL_write(sssl, msg, strlen(msg)) > 0) {
        printf("Unexpected success writing data\n");
        goto err;
    }

    ret = 1;
 err:
    SSL_free(sssl);
    SSL_free(cssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return ret;
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

    ADD_TEST(test_fatalerr);

    testresult = run_tests(argv[0]);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(err) <= 0)
        testresult = 1;
#endif
    BIO_free(err);

    if (!testresult)
        printf("PASS\n");

    return testresult;
}
