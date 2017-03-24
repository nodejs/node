/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#define USE_SOCKETS
#include "ssl_locl.h"

int ssl3_do_change_cipher_spec(SSL *s)
{
    int i;
    const char *sender;
    int slen;

    if (s->server)
        i = SSL3_CHANGE_CIPHER_SERVER_READ;
    else
        i = SSL3_CHANGE_CIPHER_CLIENT_READ;

    if (s->s3->tmp.key_block == NULL) {
        if (s->session == NULL || s->session->master_key_length == 0) {
            /* might happen if dtls1_read_bytes() calls this */
            SSLerr(SSL_F_SSL3_DO_CHANGE_CIPHER_SPEC, SSL_R_CCS_RECEIVED_EARLY);
            return (0);
        }

        s->session->cipher = s->s3->tmp.new_cipher;
        if (!s->method->ssl3_enc->setup_key_block(s))
            return (0);
    }

    if (!s->method->ssl3_enc->change_cipher_state(s, i))
        return (0);

    /*
     * we have to record the message digest at this point so we can get it
     * before we read the finished message
     */
    if (!s->server) {
        sender = s->method->ssl3_enc->server_finished_label;
        slen = s->method->ssl3_enc->server_finished_label_len;
    } else {
        sender = s->method->ssl3_enc->client_finished_label;
        slen = s->method->ssl3_enc->client_finished_label_len;
    }

    i = s->method->ssl3_enc->final_finish_mac(s,
                                              sender, slen,
                                              s->s3->tmp.peer_finish_md);
    if (i == 0) {
        SSLerr(SSL_F_SSL3_DO_CHANGE_CIPHER_SPEC, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    s->s3->tmp.peer_finish_md_len = i;

    return (1);
}

int ssl3_send_alert(SSL *s, int level, int desc)
{
    /* Map tls/ssl alert value to correct one */
    desc = s->method->ssl3_enc->alert_value(desc);
    if (s->version == SSL3_VERSION && desc == SSL_AD_PROTOCOL_VERSION)
        desc = SSL_AD_HANDSHAKE_FAILURE; /* SSL 3.0 does not have
                                          * protocol_version alerts */
    if (desc < 0)
        return -1;
    /* If a fatal one, remove from cache */
    if ((level == SSL3_AL_FATAL) && (s->session != NULL))
        SSL_CTX_remove_session(s->session_ctx, s->session);

    s->s3->alert_dispatch = 1;
    s->s3->send_alert[0] = level;
    s->s3->send_alert[1] = desc;
    if (!RECORD_LAYER_write_pending(&s->rlayer)) {
        /* data still being written out? */
        return s->method->ssl_dispatch_alert(s);
    }
    /*
     * else data is still being written out, we will get written some time in
     * the future
     */
    return -1;
}

int ssl3_dispatch_alert(SSL *s)
{
    int i, j;
    unsigned int alertlen;
    void (*cb) (const SSL *ssl, int type, int val) = NULL;

    s->s3->alert_dispatch = 0;
    alertlen = 2;
    i = do_ssl3_write(s, SSL3_RT_ALERT, &s->s3->send_alert[0], &alertlen, 1, 0);
    if (i <= 0) {
        s->s3->alert_dispatch = 1;
    } else {
        /*
         * Alert sent to BIO.  If it is important, flush it now. If the
         * message does not get sent due to non-blocking IO, we will not
         * worry too much.
         */
        if (s->s3->send_alert[0] == SSL3_AL_FATAL)
            (void)BIO_flush(s->wbio);

        if (s->msg_callback)
            s->msg_callback(1, s->version, SSL3_RT_ALERT, s->s3->send_alert,
                            2, s, s->msg_callback_arg);

        if (s->info_callback != NULL)
            cb = s->info_callback;
        else if (s->ctx->info_callback != NULL)
            cb = s->ctx->info_callback;

        if (cb != NULL) {
            j = (s->s3->send_alert[0] << 8) | s->s3->send_alert[1];
            cb(s, SSL_CB_WRITE_ALERT, j);
        }
    }
    return (i);
}
