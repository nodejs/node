/*
 * Copyright 2023-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include "ossl-nghttp3.h"
#include <openssl/err.h>

static int done;

static void make_nv(nghttp3_nv *nv, const char *name, const char *value)
{
    nv->name        = (uint8_t *)name;
    nv->value       = (uint8_t *)value;
    nv->namelen     = strlen(name);
    nv->valuelen    = strlen(value);
    nv->flags       = NGHTTP3_NV_FLAG_NONE;
}

static int on_recv_header(nghttp3_conn *h3conn, int64_t stream_id,
                          int32_t token,
                          nghttp3_rcbuf *name, nghttp3_rcbuf *value,
                          uint8_t flags,
                          void *conn_user_data,
                          void *stream_user_data)
{
    nghttp3_vec vname, vvalue;

    /* Received a single HTTP header. */
    vname   = nghttp3_rcbuf_get_buf(name);
    vvalue  = nghttp3_rcbuf_get_buf(value);

    fwrite(vname.base, vname.len, 1, stderr);
    fprintf(stderr, ": ");
    fwrite(vvalue.base, vvalue.len, 1, stderr);
    fprintf(stderr, "\n");

    return 0;
}

static int on_end_headers(nghttp3_conn *h3conn, int64_t stream_id,
                          int fin,
                          void *conn_user_data, void *stream_user_data)
{
    fprintf(stderr, "\n");
    return 0;
}

static int on_recv_data(nghttp3_conn *h3conn, int64_t stream_id,
                        const uint8_t *data, size_t datalen,
                        void *conn_user_data, void *stream_user_data)
{
    size_t wr;

    /* HTTP response body data - write it to stdout. */
    while (datalen > 0) {
        wr = fwrite(data, 1, datalen, stdout);
        if (ferror(stdout))
            return 1;

        data    += wr;
        datalen -= wr;
    }

    return 0;
}

static int on_end_stream(nghttp3_conn *h3conn, int64_t stream_id,
                         void *conn_user_data, void *stream_user_data)
{
    /* HTTP transaction is done - set done flag so that we stop looping. */
    done = 1;
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 1;
    SSL_CTX *ctx = NULL;
    OSSL_DEMO_H3_CONN *conn = NULL;
    nghttp3_nv nva[16];
    nghttp3_callbacks callbacks = {0};
    size_t num_nv = 0;
    const char *addr;

    /* Check arguments. */
    if (argc < 2) {
        fprintf(stderr, "usage: %s <host:port>\n", argv[0]);
        goto err;
    }

    addr = argv[1];

    /* Setup SSL_CTX. */
    if ((ctx = SSL_CTX_new(OSSL_QUIC_client_method())) == NULL)
        goto err;

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    if (SSL_CTX_set_default_verify_paths(ctx) == 0)
        goto err;

    /* Setup callbacks. */
    callbacks.recv_header   = on_recv_header;
    callbacks.end_headers   = on_end_headers;
    callbacks.recv_data     = on_recv_data;
    callbacks.end_stream    = on_end_stream;

    /* Create connection. */
    if ((conn = OSSL_DEMO_H3_CONN_new_for_addr(ctx, addr, &callbacks,
                                               NULL, NULL)) == NULL) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_OPERATION_FAIL,
                       "cannot create HTTP/3 connection");
        goto err;
    }

    /* Build HTTP headers. */
    make_nv(&nva[num_nv++], ":method", "GET");
    make_nv(&nva[num_nv++], ":scheme", "https");
    make_nv(&nva[num_nv++], ":authority", addr);
    make_nv(&nva[num_nv++], ":path", "/");
    make_nv(&nva[num_nv++], "user-agent", "OpenSSL-Demo/nghttp3");

    /* Submit request. */
    if (!OSSL_DEMO_H3_CONN_submit_request(conn, nva, num_nv, NULL, NULL)) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_OPERATION_FAIL,
                       "cannot submit HTTP/3 request");
        goto err;
    }

    /* Wait for request to complete. */
    while (!done)
        if (!OSSL_DEMO_H3_CONN_handle_events(conn)) {
            ERR_raise_data(ERR_LIB_USER, ERR_R_OPERATION_FAIL,
                           "cannot handle events");
            goto err;
        }

    ret = 0;
err:
    if (ret != 0)
        ERR_print_errors_fp(stderr);

    OSSL_DEMO_H3_CONN_free(conn);
    SSL_CTX_free(ctx);
    return ret;
}
