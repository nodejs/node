/*
 * Copyright 2013-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>

int main(int argc, char **argv)
{
    BIO *sbio = NULL, *out = NULL;
    int i, len, rv;
    char tmpbuf[1024];
    SSL_CTX *ctx = NULL;
    SSL_CONF_CTX *cctx = NULL;
    SSL *ssl = NULL;
    CONF *conf = NULL;
    STACK_OF(CONF_VALUE) *sect = NULL;
    CONF_VALUE *cnf;
    const char *connect_str = "localhost:4433";
    long errline = -1;

    conf = NCONF_new(NULL);

    if (NCONF_load(conf, "connect.cnf", &errline) <= 0) {
        if (errline <= 0)
            fprintf(stderr, "Error processing config file\n");
        else
            fprintf(stderr, "Error on line %ld\n", errline);
        goto end;
    }

    sect = NCONF_get_section(conf, "default");

    if (sect == NULL) {
        fprintf(stderr, "Error retrieving default section\n");
        goto end;
    }

    ctx = SSL_CTX_new(TLS_client_method());
    cctx = SSL_CONF_CTX_new();
    SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_CLIENT);
    SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_FILE);
    SSL_CONF_CTX_set_ssl_ctx(cctx, ctx);
    for (i = 0; i < sk_CONF_VALUE_num(sect); i++) {
        cnf = sk_CONF_VALUE_value(sect, i);
        rv = SSL_CONF_cmd(cctx, cnf->name, cnf->value);
        if (rv > 0)
            continue;
        if (rv != -2) {
            fprintf(stderr, "Error processing %s = %s\n",
                    cnf->name, cnf->value);
            ERR_print_errors_fp(stderr);
            goto end;
        }
        if (strcmp(cnf->name, "Connect") == 0) {
            connect_str = cnf->value;
        } else {
            fprintf(stderr, "Unknown configuration option %s\n", cnf->name);
            goto end;
        }
    }

    if (!SSL_CONF_CTX_finish(cctx)) {
        fprintf(stderr, "Finish error\n");
        ERR_print_errors_fp(stderr);
        goto end;
    }

    /*
     * We'd normally set some stuff like the verify paths and * mode here
     * because as things stand this will connect to * any server whose
     * certificate is signed by any CA.
     */

    sbio = BIO_new_ssl_connect(ctx);

    BIO_get_ssl(sbio, &ssl);

    if (!ssl) {
        fprintf(stderr, "Can't locate SSL pointer\n");
        goto end;
    }

    /* Don't want any retries */
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    /* We might want to do other things with ssl here */

    BIO_set_conn_hostname(sbio, connect_str);

    out = BIO_new_fp(stdout, BIO_NOCLOSE);
    if (BIO_do_connect(sbio) <= 0) {
        fprintf(stderr, "Error connecting to server\n");
        ERR_print_errors_fp(stderr);
        goto end;
    }

    if (BIO_do_handshake(sbio) <= 0) {
        fprintf(stderr, "Error establishing SSL connection\n");
        ERR_print_errors_fp(stderr);
        goto end;
    }

    /* Could examine ssl here to get connection info */

    BIO_puts(sbio, "GET / HTTP/1.0\n\n");
    for (;;) {
        len = BIO_read(sbio, tmpbuf, 1024);
        if (len <= 0)
            break;
        BIO_write(out, tmpbuf, len);
    }
 end:
    SSL_CONF_CTX_free(cctx);
    BIO_free_all(sbio);
    BIO_free(out);
    NCONF_free(conf);
    return 0;
}
