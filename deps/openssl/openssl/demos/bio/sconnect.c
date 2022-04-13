/*
 * Copyright 1998-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * A minimal program to do SSL to a passed host and port.
 * It is actually using non-blocking IO but in a very simple manner
 * sconnect host:port - it does a 'GET / HTTP/1.0'
 *
 * cc -I../../include sconnect.c -L../.. -lssl -lcrypto
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#define HOSTPORT "localhost:4433"
#define CAFILE "root.pem"

int main(int argc, char *argv[])
{
    const char *hostport = HOSTPORT;
    const char *CAfile = CAFILE;
    const char *hostname;
    char *cp;
    BIO *out = NULL;
    char buf[1024 * 10], *p;
    SSL_CTX *ssl_ctx = NULL;
    SSL *ssl;
    BIO *ssl_bio;
    int i, len, off, ret = EXIT_FAILURE;

    if (argc > 1)
        hostport = argv[1];
    if (argc > 2)
        CAfile = argv[2];

#ifdef WATT32
    dbug_init();
    sock_init();
#endif

    ssl_ctx = SSL_CTX_new(TLS_client_method());

    /* Enable trust chain verification */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_load_verify_locations(ssl_ctx, CAfile, NULL);

    /* Lets make a SSL structure */
    ssl = SSL_new(ssl_ctx);
    SSL_set_connect_state(ssl);


    /* Use it inside an SSL BIO */
    ssl_bio = BIO_new(BIO_f_ssl());
    BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);

    /* Lets use a connect BIO under the SSL BIO */
    out = BIO_new(BIO_s_connect());
    BIO_set_conn_hostname(out, hostport);

    /* The BIO has parsed the host:port and even IPv6 literals in [] */
    hostname = BIO_get_conn_hostname(out);
    if (!hostname || SSL_set1_host(ssl, hostname) <= 0)
        goto err;

    BIO_set_nbio(out, 1);
    out = BIO_push(ssl_bio, out);

    p = "GET / HTTP/1.0\r\n\r\n";
    len = strlen(p);

    off = 0;
    for (;;) {
        i = BIO_write(out, &(p[off]), len);
        if (i <= 0) {
            if (BIO_should_retry(out)) {
                fprintf(stderr, "write DELAY\n");
                sleep(1);
                continue;
            } else {
                goto err;
            }
        }
        off += i;
        len -= i;
        if (len <= 0)
            break;
    }

    for (;;) {
        i = BIO_read(out, buf, sizeof(buf));
        if (i == 0)
            break;
        if (i < 0) {
            if (BIO_should_retry(out)) {
                fprintf(stderr, "read DELAY\n");
                sleep(1);
                continue;
            }
            goto err;
        }
        fwrite(buf, 1, i, stdout);
    }

    ret = EXIT_SUCCESS;
    goto done;

 err:
    if (ERR_peek_error() == 0) { /* system call error */
        fprintf(stderr, "errno=%d ", errno);
        perror("error");
    } else {
        ERR_print_errors_fp(stderr);
    }
 done:
    BIO_free_all(out);
    SSL_CTX_free(ssl_ctx);
    return ret;
}
