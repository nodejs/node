/*
 * Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssltestlib.h"

int main(int argc, char *argv[])
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *sssl = NULL, *cssl = NULL;
    const char *msg = "Dummy";
    BIO *err = NULL, *wbio = NULL;
    int ret = 1, len;
    char buf[80];
    unsigned char dummyrec[] = {
        0x17, 0x03, 0x03, 0x00, 0x05, 'D', 'u', 'm', 'm', 'y'
    };

    if (argc != 3) {
        printf("Incorrect number of parameters\n");
        return 1;
    }

    SSL_library_init();
    SSL_load_error_strings();
    err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);
    CRYPTO_malloc_debug_init();
    CRYPTO_set_mem_debug_options(V_CRYPTO_MDEBUG_ALL);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    if (!create_ssl_ctx_pair(SSLv23_method(), SSLv23_method(), &sctx, &cctx,
                             argv[1], argv[2])) {
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

    ret = 0;
 err:
    SSL_free(sssl);
    SSL_free(cssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    ERR_print_errors_fp(stderr);

    if (ret) {
        printf("Fatal err test: FAILED\n");
    }

    ERR_free_strings();
    ERR_remove_thread_state(NULL);
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    CRYPTO_mem_leaks(err);
    BIO_free(err);

    return ret;
}
