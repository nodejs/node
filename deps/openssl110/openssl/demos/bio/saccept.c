/*
 * Copyright 1998-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * A minimal program to serve an SSL connection.
 * It uses blocking.
 * saccept host:port
 * host is the interface IP to use.  If any interface, use *:port
 * The default it *:4433
 *
 * cc -I../../include saccept.c -L../.. -lssl -lcrypto -ldl
 */

#include <stdio.h>
#include <signal.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#define CERT_FILE       "server.pem"

static int done = 0;

void interrupt(int sig)
{
    done = 1;
}

void sigsetup(void)
{
    struct sigaction sa;

    /*
     * Catch at most once, and don't restart the accept system call.
     */
    sa.sa_flags = SA_RESETHAND;
    sa.sa_handler = interrupt;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
}

int main(int argc, char *argv[])
{
    char *port = NULL;
    BIO *in = NULL;
    BIO *ssl_bio, *tmp;
    SSL_CTX *ctx;
    char buf[512];
    int ret = 1, i;

    if (argc <= 1)
        port = "*:4433";
    else
        port = argv[1];

    ctx = SSL_CTX_new(TLS_server_method());
    if (!SSL_CTX_use_certificate_chain_file(ctx, CERT_FILE))
        goto err;
    if (!SSL_CTX_use_PrivateKey_file(ctx, CERT_FILE, SSL_FILETYPE_PEM))
        goto err;
    if (!SSL_CTX_check_private_key(ctx))
        goto err;

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

    /* Arrange to leave server loop on interrupt */
    sigsetup();

 again:
    /*
     * The first call will setup the accept socket, and the second will get a
     * socket.  In this loop, the first actual accept will occur in the
     * BIO_read() function.
     */

    if (BIO_do_accept(in) <= 0)
        goto err;

    while (!done) {
        i = BIO_read(in, buf, 512);
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
        if (i < 0)
            goto err;
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
