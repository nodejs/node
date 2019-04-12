/*
 * Copyright 2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "ssl_local.h"
#include "internal/cryptlib.h"
#include "internal/refcount.h"

#ifdef OPENSSL_NO_QUIC
NON_EMPTY_TRANSLATION_UNIT
#else

int SSL_set_quic_transport_params(SSL *ssl, const uint8_t *params,
                                  size_t params_len)
{
    uint8_t *tmp;

    if (params == NULL || params_len == 0) {
        tmp = NULL;
        params_len = 0;
    } else {
        tmp = OPENSSL_memdup(params, params_len);
        if (tmp == NULL)
            return 0;
    }

    OPENSSL_free(ssl->ext.quic_transport_params);
    ssl->ext.quic_transport_params = tmp;
    ssl->ext.quic_transport_params_len = params_len;
    return 1;
}

void SSL_get_peer_quic_transport_params(const SSL *ssl,
                                        const uint8_t **out_params,
                                        size_t *out_params_len)
{
    *out_params = ssl->ext.peer_quic_transport_params;
    *out_params_len = ssl->ext.peer_quic_transport_params_len;
}

size_t SSL_quic_max_handshake_flight_len(const SSL *ssl, OSSL_ENCRYPTION_LEVEL level)
{
    /*
     * Limits flights to 16K by default when there are no large
     * (certificate-carrying) messages.
     */
    static const size_t DEFAULT_FLIGHT_LIMIT = 16384;

    switch (level) {
    case ssl_encryption_initial:
        return DEFAULT_FLIGHT_LIMIT;
    case ssl_encryption_early_data:
        /* QUIC does not send EndOfEarlyData. */
        return 0;
    case ssl_encryption_handshake:
        if (ssl->server) {
            /*
             * Servers may receive Certificate message if configured to request
             * client certificates.
             */
            if ((ssl->verify_mode & SSL_VERIFY_PEER)
                    && ssl->max_cert_list > DEFAULT_FLIGHT_LIMIT)
                return ssl->max_cert_list;
        } else {
            /*
             * Clients may receive both Certificate message and a CertificateRequest
             * message.
             */
            if (2*ssl->max_cert_list > DEFAULT_FLIGHT_LIMIT)
                return 2 * ssl->max_cert_list;
        }
        return DEFAULT_FLIGHT_LIMIT;
    case ssl_encryption_application:
        return DEFAULT_FLIGHT_LIMIT;
    }

    return 0;
}

OSSL_ENCRYPTION_LEVEL SSL_quic_read_level(const SSL *ssl)
{
    return ssl->quic_read_level;
}

OSSL_ENCRYPTION_LEVEL SSL_quic_write_level(const SSL *ssl)
{
    return ssl->quic_write_level;
}

int SSL_provide_quic_data(SSL *ssl, OSSL_ENCRYPTION_LEVEL level,
                          const uint8_t *data, size_t len)
{
    size_t l;

    if (!SSL_IS_QUIC(ssl)) {
        SSLerr(SSL_F_SSL_PROVIDE_QUIC_DATA, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }

    /* Level can be different than the current read, but not less */
    if (level < ssl->quic_read_level
            || (ssl->quic_input_data_tail != NULL && level < ssl->quic_input_data_tail->level)) {
        SSLerr(SSL_F_SSL_PROVIDE_QUIC_DATA, SSL_R_WRONG_ENCRYPTION_LEVEL_RECEIVED);
        return 0;
    }

    /* Split the QUIC messages up, if necessary */
    while (len > 0) {
        QUIC_DATA *qd;
        const uint8_t *p = data + 1;

        /* Check for an incomplete block */
        qd = ssl->quic_input_data_tail;
        if (qd != NULL) {
            l = qd->length - qd->offset;
            if (l != 0) {
                /* we still need to copy `l` bytes into the last data block */
                if (l > len)
                    l = len;
                memcpy((char*)(qd+1) + qd->offset, data, l);
                qd->offset += l;
                len -= l;
                data += l;
                continue;
            }
        }

        n2l3(p, l);
        l += SSL3_HM_HEADER_LENGTH;

        qd = OPENSSL_zalloc(sizeof(QUIC_DATA) + l);
        if (qd == NULL) {
            SSLerr(SSL_F_SSL_PROVIDE_QUIC_DATA, SSL_R_INTERNAL_ERROR);
            return 0;
        }

        qd->next = NULL;
        qd->length = l;
        qd->level = level;
        /* partial data received? */
        if (l > len)
            l = len;
        qd->offset = l;

        memcpy((void*)(qd + 1), data, l);
        if (ssl->quic_input_data_tail != NULL)
            ssl->quic_input_data_tail->next = qd;
        else
            ssl->quic_input_data_head = qd;
        ssl->quic_input_data_tail = qd;

        data += l;
        len -= l;
    }

    return 1;
}

int SSL_CTX_set_quic_method(SSL_CTX *ctx, const SSL_QUIC_METHOD *quic_method)
{
    switch (ctx->method->version) {
    case DTLS1_VERSION:
    case DTLS1_2_VERSION:
    case DTLS_ANY_VERSION:
    case DTLS1_BAD_VER:
        return 0;
    default:
        break;
    }
    ctx->quic_method = quic_method;
    ctx->options &= ~SSL_OP_ENABLE_MIDDLEBOX_COMPAT;
    return 1;
}

int SSL_set_quic_method(SSL *ssl, const SSL_QUIC_METHOD *quic_method)
{
    switch (ssl->method->version) {
    case DTLS1_VERSION:
    case DTLS1_2_VERSION:
    case DTLS_ANY_VERSION:
    case DTLS1_BAD_VER:
        return 0;
    default:
        break;
    }
    ssl->quic_method = quic_method;
    ssl->options &= ~SSL_OP_ENABLE_MIDDLEBOX_COMPAT;
    return 1;
}

int quic_set_encryption_secrets(SSL *ssl, OSSL_ENCRYPTION_LEVEL level)
{
    uint8_t *c2s_secret = NULL;
    uint8_t *s2c_secret = NULL;
    size_t len;
    const EVP_MD *md;

    if (!SSL_IS_QUIC(ssl))
        return 1;

    /* secrets from the POV of the client */
    switch (level) {
    case ssl_encryption_early_data:
        c2s_secret = ssl->client_early_traffic_secret;
        break;
    case ssl_encryption_handshake:
        c2s_secret = ssl->client_hand_traffic_secret;
        s2c_secret = ssl->server_hand_traffic_secret;
        break;
    case ssl_encryption_application:
        c2s_secret = ssl->client_app_traffic_secret;
        s2c_secret = ssl->server_app_traffic_secret;
        break;
    default:
        return 1;
    }

    md = ssl_handshake_md(ssl);
    if (md == NULL) {
        /* May not have selected cipher, yet */
        const SSL_CIPHER *c = NULL;

        if (ssl->session != NULL)
            c = SSL_SESSION_get0_cipher(ssl->session);
        else if (ssl->psksession != NULL)
            c = SSL_SESSION_get0_cipher(ssl->psksession);

        if (c != NULL)
            md = SSL_CIPHER_get_handshake_digest(c);
    }

    if ((len = EVP_MD_size(md)) <= 0) {
        SSLfatal(ssl, SSL_AD_INTERNAL_ERROR, SSL_F_QUIC_SET_ENCRYPTION_SECRETS,
                 ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (ssl->server) {
        if (!ssl->quic_method->set_encryption_secrets(ssl, level, c2s_secret,
                                                      s2c_secret, len)) {
            SSLfatal(ssl, SSL_AD_INTERNAL_ERROR, SSL_F_QUIC_SET_ENCRYPTION_SECRETS,
                     ERR_R_INTERNAL_ERROR);
            return 0;
        }
    } else {
        if (!ssl->quic_method->set_encryption_secrets(ssl, level, s2c_secret,
                                                      c2s_secret, len)) {
            SSLfatal(ssl, SSL_AD_INTERNAL_ERROR, SSL_F_QUIC_SET_ENCRYPTION_SECRETS,
                     ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }

    return 1;
}

int SSL_process_quic_post_handshake(SSL *ssl)
{
    int ret;

    if (SSL_in_init(ssl) || !SSL_IS_QUIC(ssl)) {
        SSLerr(SSL_F_SSL_PROCESS_QUIC_POST_HANDSHAKE, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }

    ossl_statem_set_in_init(ssl, 1);
    ret = ssl->handshake_func(ssl);
    ossl_statem_set_in_init(ssl, 0);

    if (ret <= 0)
        return 0;
    return 1;
}

int SSL_is_quic(SSL* ssl)
{
    return SSL_IS_QUIC(ssl);
}

#endif
