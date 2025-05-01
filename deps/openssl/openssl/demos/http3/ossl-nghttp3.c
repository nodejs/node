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
#include <assert.h>

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

enum {
    OSSL_DEMO_H3_STREAM_TYPE_CTRL_SEND,
    OSSL_DEMO_H3_STREAM_TYPE_QPACK_ENC_SEND,
    OSSL_DEMO_H3_STREAM_TYPE_QPACK_DEC_SEND,
    OSSL_DEMO_H3_STREAM_TYPE_REQ,
};

#define BUF_SIZE    4096

struct ossl_demo_h3_stream_st {
    uint64_t            id;             /* QUIC stream ID */
    SSL                 *s;             /* QUIC stream SSL object */
    int                 done_recv_fin;  /* Received FIN */
    void                *user_data;

    uint8_t             buf[BUF_SIZE];
    size_t              buf_cur, buf_total;
};

DEFINE_LHASH_OF_EX(OSSL_DEMO_H3_STREAM);

static void h3_stream_free(OSSL_DEMO_H3_STREAM *s)
{
    if (s == NULL)
        return;

    SSL_free(s->s);
    OPENSSL_free(s);
}

static unsigned long h3_stream_hash(const OSSL_DEMO_H3_STREAM *s)
{
    return (unsigned long)s->id;
}

static int h3_stream_eq(const OSSL_DEMO_H3_STREAM *a, const OSSL_DEMO_H3_STREAM *b)
{
    if (a->id < b->id) return -1;
    if (a->id > b->id) return 1;
    return 0;
}

void *OSSL_DEMO_H3_STREAM_get_user_data(const OSSL_DEMO_H3_STREAM *s)
{
    return s->user_data;
}

struct ossl_demo_h3_conn_st {
    /* QUIC connection SSL object */
    SSL                             *qconn;
    /* BIO wrapping QCSO */
    BIO                             *qconn_bio;
    /* HTTP/3 connection object */
    nghttp3_conn                    *h3conn;
    /* map of stream IDs to OSSL_DEMO_H3_STREAMs */
    LHASH_OF(OSSL_DEMO_H3_STREAM)   *streams;
    /* opaque user data pointer */
    void                            *user_data;

    int                             pump_res;
    size_t                          consumed_app_data;

    /* Forwarding callbacks */
    nghttp3_recv_data               recv_data_cb;
    nghttp3_stream_close            stream_close_cb;
    nghttp3_stop_sending            stop_sending_cb;
    nghttp3_reset_stream            reset_stream_cb;
    nghttp3_deferred_consume        deferred_consume_cb;
};

void OSSL_DEMO_H3_CONN_free(OSSL_DEMO_H3_CONN *conn)
{
    if (conn == NULL)
        return;

    lh_OSSL_DEMO_H3_STREAM_doall(conn->streams, h3_stream_free);

    nghttp3_conn_del(conn->h3conn);
    BIO_free_all(conn->qconn_bio);
    lh_OSSL_DEMO_H3_STREAM_free(conn->streams);
    OPENSSL_free(conn);
}

static OSSL_DEMO_H3_STREAM *h3_conn_create_stream(OSSL_DEMO_H3_CONN *conn, int type)
{
    OSSL_DEMO_H3_STREAM *s;
    uint64_t flags = SSL_STREAM_FLAG_ADVANCE;

    if ((s = OPENSSL_zalloc(sizeof(OSSL_DEMO_H3_STREAM))) == NULL)
        return NULL;

    if (type != OSSL_DEMO_H3_STREAM_TYPE_REQ)
        flags |= SSL_STREAM_FLAG_UNI;

    if ((s->s = SSL_new_stream(conn->qconn, flags)) == NULL) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                       "could not create QUIC stream object");
        goto err;
    }

    s->id   = SSL_get_stream_id(s->s);
    lh_OSSL_DEMO_H3_STREAM_insert(conn->streams, s);
    return s;

err:
    OPENSSL_free(s);
    return NULL;
}

static OSSL_DEMO_H3_STREAM *h3_conn_accept_stream(OSSL_DEMO_H3_CONN *conn, SSL *qstream)
{
    OSSL_DEMO_H3_STREAM *s;

    if ((s = OPENSSL_zalloc(sizeof(OSSL_DEMO_H3_STREAM))) == NULL)
        return NULL;

    s->id   = SSL_get_stream_id(qstream);
    s->s    = qstream;
    lh_OSSL_DEMO_H3_STREAM_insert(conn->streams, s);
    return s;
}

static void h3_conn_remove_stream(OSSL_DEMO_H3_CONN *conn, OSSL_DEMO_H3_STREAM *s)
{
    if (s == NULL)
        return;

    lh_OSSL_DEMO_H3_STREAM_delete(conn->streams, s);
    h3_stream_free(s);
}

static int h3_conn_recv_data(nghttp3_conn *h3conn, int64_t stream_id,
                             const uint8_t *data, size_t datalen,
                             void *conn_user_data, void *stream_user_data)
{
    OSSL_DEMO_H3_CONN *conn = conn_user_data;

    conn->consumed_app_data += datalen;
    if (conn->recv_data_cb == NULL)
        return 0;

    return conn->recv_data_cb(h3conn, stream_id, data, datalen,
                              conn_user_data, stream_user_data);
}

static int h3_conn_stream_close(nghttp3_conn *h3conn, int64_t stream_id,
                                uint64_t app_error_code,
                                void *conn_user_data, void *stream_user_data)
{
    int ret = 0;
    OSSL_DEMO_H3_CONN *conn = conn_user_data;
    OSSL_DEMO_H3_STREAM *stream = stream_user_data;

    if (conn->stream_close_cb != NULL)
        ret = conn->stream_close_cb(h3conn, stream_id, app_error_code,
                                    conn_user_data, stream_user_data);

    h3_conn_remove_stream(conn, stream);
    return ret;
}

static int h3_conn_stop_sending(nghttp3_conn *h3conn, int64_t stream_id,
                                uint64_t app_error_code,
                                void *conn_user_data, void *stream_user_data)
{
    int ret = 0;
    OSSL_DEMO_H3_CONN *conn = conn_user_data;
    OSSL_DEMO_H3_STREAM *stream = stream_user_data;

    if (conn->stop_sending_cb != NULL)
        ret = conn->stop_sending_cb(h3conn, stream_id, app_error_code,
                                    conn_user_data, stream_user_data);

    SSL_free(stream->s);
    stream->s = NULL;
    return ret;
}

static int h3_conn_reset_stream(nghttp3_conn *h3conn, int64_t stream_id,
                                uint64_t app_error_code,
                                void *conn_user_data, void *stream_user_data)
{
    int ret = 0;
    OSSL_DEMO_H3_CONN *conn = conn_user_data;
    OSSL_DEMO_H3_STREAM *stream = stream_user_data;
    SSL_STREAM_RESET_ARGS args = {0};

    if (conn->reset_stream_cb != NULL)
        ret = conn->reset_stream_cb(h3conn, stream_id, app_error_code,
                                   conn_user_data, stream_user_data);

    if (stream->s != NULL) {
        args.quic_error_code = app_error_code;

        if (!SSL_stream_reset(stream->s, &args, sizeof(args)))
            return 1;
    }

    return ret;
}

static int h3_conn_deferred_consume(nghttp3_conn *h3conn, int64_t stream_id,
                                    size_t consumed,
                                    void *conn_user_data, void *stream_user_data)
{
    int ret = 0;
    OSSL_DEMO_H3_CONN *conn = conn_user_data;

    if (conn->deferred_consume_cb != NULL)
        ret = conn->deferred_consume_cb(h3conn, stream_id, consumed,
                                        conn_user_data, stream_user_data);

    conn->consumed_app_data += consumed;
    return ret;
}

OSSL_DEMO_H3_CONN *OSSL_DEMO_H3_CONN_new_for_conn(BIO *qconn_bio,
                                                  const nghttp3_callbacks *callbacks,
                                                  const nghttp3_settings *settings,
                                                  void *user_data)
{
    int ec;
    OSSL_DEMO_H3_CONN *conn;
    OSSL_DEMO_H3_STREAM *s_ctl_send = NULL;
    OSSL_DEMO_H3_STREAM *s_qpenc_send = NULL;
    OSSL_DEMO_H3_STREAM *s_qpdec_send = NULL;
    nghttp3_settings dsettings = {0};
    nghttp3_callbacks intl_callbacks = {0};
    static const unsigned char alpn[] = {2, 'h', '3'};

    if (qconn_bio == NULL) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_PASSED_NULL_PARAMETER,
                       "QUIC connection BIO must be provided");
        return NULL;
    }

    if ((conn = OPENSSL_zalloc(sizeof(OSSL_DEMO_H3_CONN))) == NULL)
        return NULL;

    conn->qconn_bio = qconn_bio;
    conn->user_data = user_data;

    if (BIO_get_ssl(qconn_bio, &conn->qconn) == 0) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_PASSED_INVALID_ARGUMENT,
                       "BIO must be an SSL BIO");
        goto err;
    }

    /* Create the map of stream IDs to OSSL_DEMO_H3_STREAM structures. */
    if ((conn->streams = lh_OSSL_DEMO_H3_STREAM_new(h3_stream_hash, h3_stream_eq)) == NULL)
        goto err;

    /*
     * If the application has not started connecting yet, helpfully
     * auto-configure ALPN. If the application wants to initiate the connection
     * itself, it must take care of this itself.
     */
    if (SSL_in_before(conn->qconn))
        if (SSL_set_alpn_protos(conn->qconn, alpn, sizeof(alpn))) {
            /* SSL_set_alpn_protos returns 1 on failure */
            ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                           "failed to configure ALPN");
            goto err;
        }

    /*
     * We use the QUIC stack in non-blocking mode so that we can react to
     * incoming data on different streams, and e.g. incoming streams initiated
     * by a server, as and when events occur.
     */
    BIO_set_nbio(conn->qconn_bio, 1);

    /*
     * Disable default stream mode and create all streams explicitly. Each QUIC
     * stream will be represented by its own QUIC stream SSL object (QSSO). This
     * also automatically enables us to accept incoming streams (see
     * SSL_set_incoming_stream_policy(3)).
     */
    if (!SSL_set_default_stream_mode(conn->qconn, SSL_DEFAULT_STREAM_MODE_NONE)) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                       "failed to configure default stream mode");
        goto err;
    }

    /*
     * HTTP/3 requires a couple of unidirectional management streams: a control
     * stream and some QPACK state management streams for each side of a
     * connection. These are the instances on our side (with us sending); the
     * server will also create its own equivalent unidirectional streams on its
     * side, which we handle subsequently as they come in (see SSL_accept_stream
     * in the event handling code below).
     */
    if ((s_ctl_send
            = h3_conn_create_stream(conn, OSSL_DEMO_H3_STREAM_TYPE_CTRL_SEND)) == NULL)
        goto err;

    if ((s_qpenc_send
            = h3_conn_create_stream(conn, OSSL_DEMO_H3_STREAM_TYPE_QPACK_ENC_SEND)) == NULL)
        goto err;

    if ((s_qpdec_send
            = h3_conn_create_stream(conn, OSSL_DEMO_H3_STREAM_TYPE_QPACK_DEC_SEND)) == NULL)
        goto err;

    if (settings == NULL) {
        nghttp3_settings_default(&dsettings);
        settings = &dsettings;
    }

    if (callbacks != NULL)
        intl_callbacks = *callbacks;

    /*
     * We need to do some of our own processing when many of these events occur,
     * so we note the original callback functions and forward appropriately.
     */
    conn->recv_data_cb          = intl_callbacks.recv_data;
    conn->stream_close_cb       = intl_callbacks.stream_close;
    conn->stop_sending_cb       = intl_callbacks.stop_sending;
    conn->reset_stream_cb       = intl_callbacks.reset_stream;
    conn->deferred_consume_cb   = intl_callbacks.deferred_consume;

    intl_callbacks.recv_data        = h3_conn_recv_data;
    intl_callbacks.stream_close     = h3_conn_stream_close;
    intl_callbacks.stop_sending     = h3_conn_stop_sending;
    intl_callbacks.reset_stream     = h3_conn_reset_stream;
    intl_callbacks.deferred_consume = h3_conn_deferred_consume;

    /* Create the HTTP/3 client state. */
    ec = nghttp3_conn_client_new(&conn->h3conn, &intl_callbacks, settings,
                                 NULL, conn);
    if (ec < 0) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                       "cannot create nghttp3 connection: %s (%d)",
                       nghttp3_strerror(ec), ec);
        goto err;
    }

    /*
     * Tell the HTTP/3 stack which stream IDs are used for our outgoing control
     * and QPACK streams. Note that we don't have to tell the HTTP/3 stack what
     * IDs are used for incoming streams as this is inferred automatically from
     * the stream type byte which starts every incoming unidirectional stream,
     * so it will autodetect the correct stream IDs for the incoming control and
     * QPACK streams initiated by the server.
     */
    ec = nghttp3_conn_bind_control_stream(conn->h3conn, s_ctl_send->id);
    if (ec < 0) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                       "cannot bind nghttp3 control stream: %s (%d)",
                       nghttp3_strerror(ec), ec);
        goto err;
    }

    ec = nghttp3_conn_bind_qpack_streams(conn->h3conn,
                                         s_qpenc_send->id,
                                         s_qpdec_send->id);
    if (ec < 0) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                       "cannot bind nghttp3 QPACK streams: %s (%d)",
                       nghttp3_strerror(ec), ec);
        goto err;
    }

    return conn;

err:
    nghttp3_conn_del(conn->h3conn);
    h3_stream_free(s_ctl_send);
    h3_stream_free(s_qpenc_send);
    h3_stream_free(s_qpdec_send);
    lh_OSSL_DEMO_H3_STREAM_free(conn->streams);
    OPENSSL_free(conn);
    return NULL;
}

OSSL_DEMO_H3_CONN *OSSL_DEMO_H3_CONN_new_for_addr(SSL_CTX *ctx, const char *addr,
                                                  const nghttp3_callbacks *callbacks,
                                                  const nghttp3_settings *settings,
                                                  void *user_data)
{
    BIO *qconn_bio = NULL;
    SSL *qconn = NULL;
    OSSL_DEMO_H3_CONN *conn = NULL;
    const char *bare_hostname;

    /* QUIC connection setup */
    if ((qconn_bio = BIO_new_ssl_connect(ctx)) == NULL)
        goto err;

    /* Pass the 'hostname:port' string into the ssl_connect BIO. */
    if (BIO_set_conn_hostname(qconn_bio, addr) == 0)
        goto err;

    /*
     * Get the 'bare' hostname out of the ssl_connect BIO. This is the hostname
     * without the port.
     */
    bare_hostname = BIO_get_conn_hostname(qconn_bio);
    if (bare_hostname == NULL)
        goto err;

    if (BIO_get_ssl(qconn_bio, &qconn) == 0)
        goto err;

    /* Set the hostname we will validate the X.509 certificate against. */
    if (SSL_set1_host(qconn, bare_hostname) <= 0)
        goto err;

    /* Configure SNI */
    if (!SSL_set_tlsext_host_name(qconn, bare_hostname))
        goto err;

    conn = OSSL_DEMO_H3_CONN_new_for_conn(qconn_bio, callbacks,
                                          settings, user_data);
    if (conn == NULL)
        goto err;

    return conn;

err:
    BIO_free_all(qconn_bio);
    return NULL;
}

int OSSL_DEMO_H3_CONN_connect(OSSL_DEMO_H3_CONN *conn)
{
    return SSL_connect(OSSL_DEMO_H3_CONN_get0_connection(conn));
}

void *OSSL_DEMO_H3_CONN_get_user_data(const OSSL_DEMO_H3_CONN *conn)
{
    return conn->user_data;
}

SSL *OSSL_DEMO_H3_CONN_get0_connection(const OSSL_DEMO_H3_CONN *conn)
{
    return conn->qconn;
}

/* Pumps received data to the HTTP/3 stack for a single stream. */
static void h3_conn_pump_stream(OSSL_DEMO_H3_STREAM *s, void *conn_)
{
    int ec;
    OSSL_DEMO_H3_CONN *conn = conn_;
    size_t num_bytes, consumed;
    uint64_t aec;

    if (!conn->pump_res)
        /*
         * Handling of a previous stream in the iteration over all streams
         * failed, so just do nothing.
         */
        return;

    for (;;) {
        if (s->s == NULL /* If we already did STOP_SENDING, ignore this stream. */
            /* If this is a write-only stream, there is no read data to check. */
            || SSL_get_stream_read_state(s->s) == SSL_STREAM_STATE_WRONG_DIR
            /*
             * If we already got a FIN for this stream, there is nothing more to
             * do for it.
             */
            || s->done_recv_fin)
            break;

        /*
         * Pump data from OpenSSL QUIC to the HTTP/3 stack by calling SSL_peek
         * to get received data and passing it to nghttp3 using
         * nghttp3_conn_read_stream. Note that this function is confusingly
         * named and inputs data to the HTTP/3 stack.
         */
        if (s->buf_cur == s->buf_total) {
            /* Need more data. */
            ec = SSL_read_ex(s->s, s->buf, sizeof(s->buf), &num_bytes);
            if (ec <= 0) {
                num_bytes = 0;
                if (SSL_get_error(s->s, ec) == SSL_ERROR_ZERO_RETURN) {
                    /* Stream concluded normally. Pass FIN to HTTP/3 stack. */
                    ec = nghttp3_conn_read_stream(conn->h3conn, s->id, NULL, 0,
                                                  /*fin=*/1);
                    if (ec < 0) {
                        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                                       "cannot pass FIN to nghttp3: %s (%d)",
                                       nghttp3_strerror(ec), ec);
                        goto err;
                    }

                    s->done_recv_fin = 1;
                } else if (SSL_get_stream_read_state(s->s)
                            == SSL_STREAM_STATE_RESET_REMOTE) {
                    /* Stream was reset by peer. */
                    if (!SSL_get_stream_read_error_code(s->s, &aec))
                        goto err;

                    ec = nghttp3_conn_close_stream(conn->h3conn, s->id, aec);
                    if (ec < 0) {
                        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                                       "cannot mark stream as reset: %s (%d)",
                                       nghttp3_strerror(ec), ec);
                        goto err;
                    }

                    s->done_recv_fin = 1;
                } else {
                    /* Other error. */
                    goto err;
                }
            }

            s->buf_cur      = 0;
            s->buf_total    = num_bytes;
        }

        if (s->buf_cur == s->buf_total)
            break;

        /*
         * This function is confusingly named as it is is named from nghttp3's
         * 'perspective'; it is used to pass data *into* the HTTP/3 stack which
         * has been received from the network.
         */
        assert(conn->consumed_app_data == 0);
        ec = nghttp3_conn_read_stream(conn->h3conn, s->id, s->buf + s->buf_cur,
                                      s->buf_total - s->buf_cur, /*fin=*/0);
        if (ec < 0) {
            ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                           "nghttp3 failed to process incoming data: %s (%d)",
                           nghttp3_strerror(ec), ec);
            goto err;
        }

        /*
         * read_stream reports the data it consumes from us in two different
         * ways; the non-application data is returned as a number of bytes 'ec'
         * above, but the number of bytes of application data has to be recorded
         * by our callback. We sum the two to determine the total number of
         * bytes which nghttp3 consumed.
         */
        consumed = ec + conn->consumed_app_data;
        assert(consumed <= s->buf_total - s->buf_cur);
        s->buf_cur += consumed;
        conn->consumed_app_data = 0;
    }

    return;
err:
    conn->pump_res = 0;
}

int OSSL_DEMO_H3_CONN_handle_events(OSSL_DEMO_H3_CONN *conn)
{
    int ec, fin;
    size_t i, num_vecs, written, total_written, total_len;
    int64_t stream_id;
    uint64_t flags;
    nghttp3_vec vecs[8] = {0};
    OSSL_DEMO_H3_STREAM key, *s;
    SSL *snew;

    if (conn == NULL)
        return 0;

    /*
     * We handle events by doing three things:
     *
     * 1. Handle new incoming streams
     * 2. Pump outgoing data from the HTTP/3 stack to the QUIC engine
     * 3. Pump incoming data from the QUIC engine to the HTTP/3 stack
     */

    /* 1. Check for new incoming streams */
    for (;;) {
        if ((snew = SSL_accept_stream(conn->qconn, SSL_ACCEPT_STREAM_NO_BLOCK)) == NULL)
            break;

        /*
         * Each new incoming stream gets wrapped into an OSSL_DEMO_H3_STREAM object and
         * added into our stream ID map.
         */
        if (h3_conn_accept_stream(conn, snew) == NULL) {
            SSL_free(snew);
            return 0;
        }
    }

    /* 2. Pump outgoing data from HTTP/3 engine to QUIC. */
    for (;;) {
        /*
         * Get a number of send vectors from the HTTP/3 engine.
         *
         * Note that this function is confusingly named as it is named from
         * nghttp3's 'perspective': this outputs pointers to data which nghttp3
         * wants to *write* to the network.
         */
        ec = nghttp3_conn_writev_stream(conn->h3conn, &stream_id, &fin,
                                        vecs, ARRAY_LEN(vecs));
        if (ec < 0)
            return 0;
        if (ec == 0)
            break;

        /*
	 * we let SSL_write_ex2(3) to conclude the stream for us (send FIN)
	 * after all data are written.
         */
        flags = (fin == 0) ? 0 : SSL_WRITE_FLAG_CONCLUDE;

        /* For each of the vectors returned, pass it to OpenSSL QUIC. */
        key.id = stream_id;
        if ((s = lh_OSSL_DEMO_H3_STREAM_retrieve(conn->streams, &key)) == NULL) {
            ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                           "no stream for ID %zd", stream_id);
            return 0;
        }

        num_vecs = ec;
        total_len = nghttp3_vec_len(vecs, num_vecs);
        total_written = 0;
        for (i = 0; i < num_vecs; ++i) {
            if (vecs[i].len == 0)
                continue;

            if (s->s == NULL) {
                /* Already did STOP_SENDING and threw away stream, ignore */
                written = vecs[i].len;
            } else if (!SSL_write_ex2(s->s, vecs[i].base, vecs[i].len, flags, &written)) {
                if (SSL_get_error(s->s, 0) == SSL_ERROR_WANT_WRITE) {
                    /*
                     * We have filled our send buffer so tell nghttp3 to stop
                     * generating more data; we have to do this explicitly.
                     */
                    written = 0;
                    nghttp3_conn_block_stream(conn->h3conn, stream_id);
                } else {
                    ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                                   "writing HTTP/3 data to network failed");
                    return 0;
                }
            } else {
                /*
                 * Tell nghttp3 it can resume generating more data in case we
                 * previously called block_stream.
                 */
                nghttp3_conn_unblock_stream(conn->h3conn, stream_id);
            }

            total_written += written;
            if (written > 0) {
                /*
                 * Tell nghttp3 we have consumed the data it output when we
                 * called writev_stream, otherwise subsequent calls to
                 * writev_stream will output the same data.
                 */
                ec = nghttp3_conn_add_write_offset(conn->h3conn, stream_id, written);
                if (ec < 0)
                    return 0;

                /*
                 * Tell nghttp3 it can free the buffered data because we will
                 * not need it again. In our case we can always do this right
                 * away because we copy the data into our QUIC send buffers
                 * rather than simply storing a reference to it.
                 */
                ec = nghttp3_conn_add_ack_offset(conn->h3conn, stream_id, written);
                if (ec < 0)
                    return 0;
            }
        }

        if (fin && total_written == total_len) {

            if (total_len == 0) {
                /*
                 * As a special case, if nghttp3 requested to write a
                 * zero-length stream with a FIN, we have to tell it we did this
                 * by calling add_write_offset(0).
                 */
                ec = nghttp3_conn_add_write_offset(conn->h3conn, stream_id, 0);
                if (ec < 0)
                    return 0;
            }
        }
    }

    /* 3. Pump incoming data from QUIC to HTTP/3 engine. */
    conn->pump_res = 1; /* cleared in below call if an error occurs */
    lh_OSSL_DEMO_H3_STREAM_doall_arg(conn->streams, h3_conn_pump_stream, conn);
    if (!conn->pump_res)
        return 0;

    return 1;
}

int OSSL_DEMO_H3_CONN_submit_request(OSSL_DEMO_H3_CONN *conn,
                                     const nghttp3_nv *nva, size_t nvlen,
                                     const nghttp3_data_reader *dr,
                                     void *user_data)
{
    int ec;
    OSSL_DEMO_H3_STREAM *s_req = NULL;

    if (conn == NULL) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_PASSED_NULL_PARAMETER,
                       "connection must be specified");
        return 0;
    }

    /* Each HTTP/3 request is represented by a stream. */
    if ((s_req = h3_conn_create_stream(conn, OSSL_DEMO_H3_STREAM_TYPE_REQ)) == NULL)
        goto err;

    s_req->user_data = user_data;

    ec = nghttp3_conn_submit_request(conn->h3conn, s_req->id, nva, nvlen,
                                     dr, s_req);
    if (ec < 0) {
        ERR_raise_data(ERR_LIB_USER, ERR_R_INTERNAL_ERROR,
                       "cannot submit HTTP/3 request: %s (%d)",
                       nghttp3_strerror(ec), ec);
        goto err;
    }

    return 1;

err:
    h3_conn_remove_stream(conn, s_req);
    return 0;
}
