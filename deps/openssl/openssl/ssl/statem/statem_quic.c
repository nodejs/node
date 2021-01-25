/*
 * Copyright 2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../ssl_local.h"
#include "statem_local.h"
#include "internal/cryptlib.h"

int quic_get_message(SSL *s, int *mt, size_t *len)
{
    size_t l;
    QUIC_DATA *qd = s->quic_input_data_head;
    uint8_t *p;

    if (qd == NULL) {
        s->rwstate = SSL_READING;
        *mt = *len = 0;
        return 0;
    }

    if (!ossl_assert(qd->length >= SSL3_HM_HEADER_LENGTH)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_QUIC_GET_MESSAGE,
                 SSL_R_BAD_LENGTH);
        *mt = *len = 0;
        return 0;
    }

    /* This is where we check for the proper level, not when data is given */
    if (qd->level != s->quic_read_level) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_QUIC_GET_MESSAGE,
                 SSL_R_WRONG_ENCRYPTION_LEVEL_RECEIVED);
        *mt = *len = 0;
        return 0;
    }

    if (!BUF_MEM_grow_clean(s->init_buf, (int)qd->length)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_QUIC_GET_MESSAGE,
                 ERR_R_BUF_LIB);
        *mt = *len = 0;
        return 0;
    }

    /* Copy buffered data */
    memcpy(s->init_buf->data, s->quic_buf->data + qd->start, qd->length);
    s->init_buf->length = qd->length;
    s->quic_input_data_head = qd->next;
    if (s->quic_input_data_head == NULL)
        s->quic_input_data_tail = NULL;
    OPENSSL_free(qd);

    s->s3->tmp.message_type = *mt = *(s->init_buf->data);
    p = (uint8_t*)s->init_buf->data + 1;
    n2l3(p, l);
    s->init_num = s->s3->tmp.message_size = *len = l;
    s->init_msg = s->init_buf->data + SSL3_HM_HEADER_LENGTH;

    /* No CCS in QUIC/TLSv1.3? */
    if (*mt == SSL3_MT_CHANGE_CIPHER_SPEC) {
        SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE,
                 SSL_F_QUIC_GET_MESSAGE,
                 SSL_R_CCS_RECEIVED_EARLY);
        *len = 0;
        return 0;
    }
    /* No KeyUpdate in QUIC */
    if (*mt == SSL3_MT_KEY_UPDATE) {
        SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_F_QUIC_GET_MESSAGE,
                 SSL_R_UNEXPECTED_MESSAGE);
        *len = 0;
        return 0;
    }


    /*
     * If receiving Finished, record MAC of prior handshake messages for
     * Finished verification.
     */
    if (*mt == SSL3_MT_FINISHED && !ssl3_take_mac(s)) {
        /* SSLfatal() already called */
        *len = 0;
        return 0;
    }

    /*
     * We defer feeding in the HRR until later. We'll do it as part of
     * processing the message
     * The TLsv1.3 handshake transcript stops at the ClientFinished
     * message.
     */
#define SERVER_HELLO_RANDOM_OFFSET  (SSL3_HM_HEADER_LENGTH + 2)
    /* KeyUpdate and NewSessionTicket do not need to be added */
    if (s->s3->tmp.message_type != SSL3_MT_NEWSESSION_TICKET
            && s->s3->tmp.message_type != SSL3_MT_KEY_UPDATE) {
        if (s->s3->tmp.message_type != SSL3_MT_SERVER_HELLO
            || s->init_num < SERVER_HELLO_RANDOM_OFFSET + SSL3_RANDOM_SIZE
            || memcmp(hrrrandom,
                      s->init_buf->data + SERVER_HELLO_RANDOM_OFFSET,
                      SSL3_RANDOM_SIZE) != 0) {
            if (!ssl3_finish_mac(s, (unsigned char *)s->init_buf->data,
                                 s->init_num + SSL3_HM_HEADER_LENGTH)) {
                /* SSLfatal() already called */
                *len = 0;
                return 0;
            }
        }
    }
    if (s->msg_callback)
        s->msg_callback(0, s->version, SSL3_RT_HANDSHAKE, s->init_buf->data,
                        (size_t)s->init_num + SSL3_HM_HEADER_LENGTH, s,
                        s->msg_callback_arg);

    return 1;
}
