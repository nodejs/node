/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * A minimal TLS server it ses SSL_CTX_config and a configuration file to
 * set most server parameters.
 */

#include <stdio.h>
#include <signal.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>

int main(int argc, char *argv[])
{
    unsigned char buf[512];
    char *port = "*:4433";
    BIO *in = NULL;
    BIO *ssl_bio, *tmp;
    SSL_CTX *ctx;
    int ret = 1, i;

    ctx = SSL_CTX_new(TLS_server_method());

    if (CONF_modules_load_file("cmod.cnf", "testapp", 0) <= 0) {
        fprintf(stderr, "Error processing config file\n");
        goto err;
    }

    if (SSL_CTX_config(ctx, "server") == 0) {
        fprintf(stderr, "Error configuring server.\n");
        goto err;
    }

    /* Setup server side SSL bio */
    ssl_bio = BIO_new_ssl(ctx, 0);

    if ((in = BIO_new_accept(port)) == NULL)
        goto err;

    /*
     * This means that when a new connection is accepted on 'in', The ssl_bio
     * will be 'duplicated' and have the new socket BIO push into it.
     * Basically it means the SSL BIO will be automatically setup
     */
    BIO_set_accept_bios(in, ssl_bio);

 again:
    /*
     * The first call will setup the accept socket, and the second will get a
     * socket.  In this loop, the first actual accept will occur in the
     * BIO_read() function.
     */

    if (BIO_do_accept(in) <= 0)
        goto err;

    for (;;) {
        i = BIO_read(in, buf, sizeof(buf));
        if (i == 0) {
            /*
             * If we have finished, remove the underlying BIO stack so the
             * next time we call any function for this BIO, it will attempt
             * to do an accept
             */
            printf("Done\n");
            tmp = BIO_pop(in);
            BIO_free_all(tmp);
            goto again;
        }
        if (i < 0) {
            if (BIO_should_retry(in))
                continue;
            goto err;
        }
        fwrite(buf, 1, i, stdout);
        fflush(stdout);
    }

    ret = 0;
 err:
    if (ret) {
        ERR_print_errors_fp(stderr);
    }
    BIO_free(in);
    exit(ret);
    return (!ret);
}
